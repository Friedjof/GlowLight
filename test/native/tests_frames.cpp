// Frame validation: tampering, foreign keys, malformed and legacy traffic.
// Nothing here may reach the application or change any session state.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

Peer makePeer(const Bytes& groupKey = testGroupKey()) {
  return Peer("aabbccddee01", "0f0e0d0c0b0a09080706050403020100", groupKey);
}

// A frame is "accepted" if it makes the device talk back to that peer or track it.
bool wasAccepted(Device& device, const Peer& peer) {
  return !device.outbox(Frame::CHALLENGE).empty() ||
         GlowSecureTestAccess::usedSessions(device.service()) > 0 ||
         device.knows(peer.nodeId());
}

void expectRejected(Device& device, Peer& peer, const Bytes& frame) {
  device.clearOutbox();
  device.deliver(frame);
  device.pump();
  CHECK(!wasAccepted(device, peer));
}

}  // namespace

// Baseline: an untouched frame is accepted, so the rejections below mean something.
GLOW_TEST(valid_frame_is_accepted) {
  Device device;
  Peer peer = makePeer();
  device.clearOutbox();
  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();
  CHECK(wasAccepted(device, peer));
}

GLOW_TEST(tampered_magic_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetMagic] = 'X';
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_version_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetVersion] = 2;
  expectRejected(device, peer, frame);
}

GLOW_TEST(nonzero_reserved_bytes_are_rejected) {
  Device device;
  Peer peer = makePeer();

  Bytes first = peer.build(Frame::HELLO, {});
  first[kOffsetReserved1] = 1;
  expectRejected(device, peer, first);

  Bytes second = peer.build(Frame::HELLO, {});
  second[kOffsetReserved2] = 1;
  expectRejected(device, peer, second);
}

GLOW_TEST(tampered_frame_type_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetType] = static_cast<uint8_t>(Frame::DATA);
  expectRejected(device, peer, frame);
}

GLOW_TEST(unknown_frame_type_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetType] = 9;
  expectRejected(device, peer, frame);

  Bytes zeroType = peer.build(Frame::HELLO, {});
  zeroType[kOffsetType] = 0;
  expectRejected(device, peer, zeroType);
}

GLOW_TEST(tampered_fragment_fields_are_rejected) {
  Device device;
  Peer peer = makePeer();

  Bytes index = peer.build(Frame::HELLO, {});
  index[kOffsetFragmentIndex] = 1;
  expectRejected(device, peer, index);

  Bytes count = peer.build(Frame::HELLO, {});
  count[kOffsetFragmentCount] = 2;
  expectRejected(device, peer, count);

  Bytes zeroCount = peer.build(Frame::HELLO, {});
  zeroCount[kOffsetFragmentCount] = 0;
  expectRejected(device, peer, zeroCount);

  Bytes tooMany = peer.build(Frame::HELLO, {});
  tooMany[kOffsetFragmentCount] = 4;
  expectRejected(device, peer, tooMany);
}

GLOW_TEST(tampered_group_tag_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetGroupTag] ^= 0x01;
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_sender_mac_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetMac + 5] ^= 0x01;
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_boot_id_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetBootId] ^= 0x01;
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_counter_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[kOffsetCounter + 7] ^= 0x01;
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_length_field_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HEARTBEAT, {});
  frame[kOffsetLength + 1] = 1;  // claims one byte of payload it does not carry
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_ciphertext_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::PROOF, Bytes(kChallengeSize, 0x42));
  frame[kHeaderSize] ^= 0x01;
  expectRejected(device, peer, frame);
}

GLOW_TEST(tampered_gcm_tag_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame[frame.size() - 1] ^= 0x01;
  expectRejected(device, peer, frame);
}

// The radio source address must match the address claimed in the header.
GLOW_TEST(source_mac_mismatch_is_rejected) {
  Device device;
  Peer peer = makePeer();
  device.clearOutbox();
  device.deliver(peer.build(Frame::HELLO, {}), "aabbccddee99");
  device.pump();
  CHECK(!wasAccepted(device, peer));
}

// A frame that claims our own MAC is a loopback or a spoof; drop it.
GLOW_TEST(frame_claiming_our_own_mac_is_rejected) {
  Device device;
  Peer impostor("983dae52877c", "00112233445566778899aabbccddeeff", testGroupKey());
  device.clearOutbox();
  device.deliver(impostor.build(Frame::HELLO, {}));
  device.pump();
  CHECK_EQ(GlowSecureTestAccess::usedSessions(device.service()), static_cast<size_t>(0));
}

// ------------------------------------------------------------ foreign key ---

GLOW_TEST(frame_from_a_different_group_key_is_rejected) {
  Device device;
  Bytes foreignKey = fromHex(std::string(64, 'b'));
  Peer stranger = makePeer(foreignKey);

  device.clearOutbox();
  device.deliver(stranger.build(Frame::HELLO, {}));
  device.deliver(stranger.build(Frame::HEARTBEAT, {}));
  device.pump();

  CHECK(!wasAccepted(device, stranger));
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(0));
}

// Correct group tag, but the payload was sealed with a foreign key: the AEAD
// must catch it even though every plaintext header check passes.
GLOW_TEST(correct_group_tag_with_foreign_ciphertext_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HEARTBEAT, {});

  Bytes foreignKey = fromHex(std::string(64, 'c'));
  Bytes wrongBootKey =
      bootKeyFor(foreignKey, frame.data() + kOffsetMac, frame.data() + kOffsetBootId);
  Bytes aad(frame.begin(), frame.begin() + kHeaderSize);
  Bytes tag;
  aesGcmEncrypt(wrongBootKey, nonceFor(peer.counter()), aad, {}, &tag);
  memcpy(frame.data() + kHeaderSize, tag.data(), kGcmTagSize);

  expectRejected(device, peer, frame);
}

// -------------------------------------------------------------- malformed ---

GLOW_TEST(plaintext_legacy_message_is_rejected) {
  Device device;
  Peer peer = makePeer();
  std::string legacy = "{\"type\":1,\"message\":{\"timestamp\":42}}";
  Bytes frame(legacy.begin(), legacy.end());
  frame.resize(kHeaderSize + kGcmTagSize + 8, 0);
  expectRejected(device, peer, frame);
}

GLOW_TEST(truncated_frame_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::HELLO, {});
  frame.resize(kHeaderSize + kGcmTagSize - 1);
  expectRejected(device, peer, frame);

  expectRejected(device, peer, Bytes());
  expectRejected(device, peer, Bytes(1, 'G'));
}

GLOW_TEST(oversized_frame_is_rejected) {
  Device device;
  Peer peer = makePeer();
  Bytes frame = peer.build(Frame::DATA, Bytes(kMaxFragmentSize, 'x'));
  frame.push_back(0);  // one byte past the ESP-NOW MTU
  expectRejected(device, peer, frame);
}

// Counter zero is never issued; the first frame after boot uses counter 1.
GLOW_TEST(zero_counter_is_rejected) {
  Device device;
  Peer peer = makePeer();
  expectRejected(device, peer, peer.buildWithCounter(0, Frame::HELLO, {}));
}

// A HELLO carries no payload; anything else is malformed.
GLOW_TEST(hello_with_payload_is_ignored) {
  Device device;
  Peer peer = makePeer();
  device.clearOutbox();
  device.deliver(peer.build(Frame::HELLO, Bytes(4, 'x')));
  device.pump();
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(0));
}
