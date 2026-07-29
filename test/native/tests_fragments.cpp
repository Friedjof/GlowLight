// Fragmentation and reassembly of payloads up to 512 bytes.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

const char kPrefix[] = "{\"type\":1,\"message\":{\"timestamp\":42,\"pad\":\"";
const char kSuffix[] = "\"}}";

struct SizedMessage {
  std::string frame;     // what goes on the air
  std::string expected;  // what the application callback should receive
};

// Builds a valid SYNC message whose serialized size is exactly totalSize.
SizedMessage sizedMessage(size_t totalSize) {
  size_t overhead = strlen(kPrefix) + strlen(kSuffix);
  CHECK(totalSize >= overhead);
  std::string padding(totalSize - overhead, 'p');

  SizedMessage message;
  message.frame = std::string(kPrefix) + padding + kSuffix;
  message.expected = "{\"timestamp\":42,\"pad\":\"" + padding + "\"}";
  CHECK_EQ(message.frame.size(), totalSize);
  return message;
}

Peer makePeer(const char* macHex = "aabbccddee01") {
  return Peer(macHex, "0f0e0d0c0b0a09080706050403020100", testGroupKey());
}

void establish(Device& device, Peer& peer) {
  peer.setCounter(100);
  CHECK(handshake(device, peer));
  device.messages.clear();
}

void deliverAll(Device& device, const std::vector<Bytes>& frames) {
  for (const Bytes& frame : frames) device.deliver(frame);
  device.pump();
}

}  // namespace

GLOW_TEST(single_fragment_message_round_trips) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(120);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  CHECK_EQ(frames.size(), static_cast<size_t>(1));

  deliverAll(device, frames);
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, message.expected);
}

GLOW_TEST(two_fragment_message_round_trips) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(300);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  CHECK_EQ(frames.size(), static_cast<size_t>(2));

  deliverAll(device, frames);
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, message.expected);
}

// Exactly at the fragment boundary: 187 bytes needs a second, one-byte fragment.
GLOW_TEST(message_one_byte_over_a_fragment_round_trips) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(kMaxFragmentSize + 1);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  CHECK_EQ(frames.size(), static_cast<size_t>(2));

  deliverAll(device, frames);
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, message.expected);
}

// The largest supported payload: the biggest mode state a lamp may broadcast.
GLOW_TEST(largest_message_round_trips_in_three_fragments) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(kMaxPlaintextSize);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  CHECK_EQ(frames.size(), static_cast<size_t>(3));

  deliverAll(device, frames);
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, message.expected);
}

GLOW_TEST(fragments_may_arrive_out_of_order) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(300);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  device.deliver(frames[1]);
  device.deliver(frames[0]);
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, message.expected);
}

GLOW_TEST(duplicate_fragment_does_not_duplicate_the_message) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(300);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  device.deliver(frames[0]);
  device.deliver(frames[0]);
  device.deliver(frames[1]);
  device.deliver(frames[1]);
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
}

GLOW_TEST(incomplete_message_is_never_delivered) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(kMaxPlaintextSize);
  std::vector<Bytes> frames = peer.buildData(message.frame);
  device.deliver(frames[0]);
  device.deliver(frames[2]);  // fragment 1 is lost
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// A lost fragment must not block the next message once reassembly times out.
GLOW_TEST(reassembly_timeout_frees_the_slot_for_the_next_message) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  std::vector<Bytes> lost = peer.buildData(sizedMessage(300).frame);
  device.deliver(lost[0]);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));

  device.advance(3000);  // reassembly timeout is two seconds

  SizedMessage next = sizedMessage(120);
  deliverAll(device, peer.buildData(next.frame));
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].second, next.expected);
}

GLOW_TEST(changing_the_fragment_count_mid_message_delivers_nothing) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(300);
  Bytes first(message.frame.begin(), message.frame.begin() + kMaxFragmentSize);
  Bytes second(message.frame.begin() + kMaxFragmentSize, message.frame.end());

  device.deliver(peer.build(Frame::DATA, first, 0, 2));
  device.deliver(peer.build(Frame::DATA, second, 1, 3));  // count changed
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// Only the last fragment may be short. Anything else is non-canonical
// fragmentation and must be refused, so a message maps to exactly one framing.
GLOW_TEST(non_canonical_fragment_sizes_are_rejected) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  SizedMessage message = sizedMessage(196);
  Bytes first(message.frame.begin(), message.frame.begin() + 10);
  Bytes second(message.frame.begin() + 10, message.frame.end());
  CHECK_EQ(second.size(), kMaxFragmentSize);

  device.deliver(peer.build(Frame::DATA, first, 0, 2));
  device.deliver(peer.build(Frame::DATA, second, 1, 2));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// A fragment index outside the declared count must never be stored.
GLOW_TEST(fragment_index_beyond_the_count_is_rejected) {
  Device device;
  Peer peer = makePeer();
  establish(device, peer);

  Bytes payload(64, 'z');
  Bytes frame = peer.build(Frame::DATA, payload, 0, 2);
  frame[kOffsetFragmentIndex] = 2;  // index 2 of 2
  device.deliver(frame);
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// Each peer gets its own reassembly slot; two interleaved messages must both
// arrive intact.
GLOW_TEST(two_peers_reassemble_independently) {
  Device device;
  Peer first = makePeer("aabbccddee01");
  Peer second = makePeer("aabbccddee02");
  establish(device, first);
  establish(device, second);

  SizedMessage firstMessage = sizedMessage(300);
  SizedMessage secondMessage = sizedMessage(260);
  std::vector<Bytes> firstFrames = first.buildData(firstMessage.frame);
  std::vector<Bytes> secondFrames = second.buildData(secondMessage.frame);

  device.deliver(firstFrames[0]);
  device.deliver(secondFrames[0]);
  device.deliver(firstFrames[1]);
  device.deliver(secondFrames[1]);
  device.pump(8);

  CHECK_EQ(device.messages.size(), static_cast<size_t>(2));
  bool sawFirst = false;
  bool sawSecond = false;
  for (const auto& message : device.messages) {
    if (message.second == firstMessage.expected) sawFirst = true;
    if (message.second == secondMessage.expected) sawSecond = true;
  }
  CHECK(sawFirst);
  CHECK(sawSecond);
}

// Fragments of an unauthenticated peer must not be buffered at all.
GLOW_TEST(fragments_before_authentication_are_dropped) {
  Device device;
  Peer peer = makePeer();

  SizedMessage message = sizedMessage(300);
  deliverAll(device, peer.buildData(message.frame));
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));

  // Authenticating afterwards must not release the buffered fragments.
  CHECK(handshake(device, peer));
  device.pump(4);
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}
