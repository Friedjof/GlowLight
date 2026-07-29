// Replay protection: the proof counter is the floor, the 64-frame window must
// never accept a counter twice, and recordings must stay useless across reboots.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

Peer makePeer() {
  return Peer("aabbccddee01", "0f0e0d0c0b0a09080706050403020100", testGroupKey());
}

std::string syncMessage(const std::string& marker = "x") {
  return "{\"type\":1,\"message\":{\"timestamp\":42,\"tag\":\"" + marker + "\"}}";
}

Bytes toBytes(const std::string& text) { return Bytes(text.begin(), text.end()); }

// Establishes a session whose proof counter is well above zero, so there is
// room below the floor for replayed recordings.
uint64_t establishAtHighCounter(Device& device, Peer& peer, uint64_t startCounter = 100) {
  peer.setCounter(startCounter);
  CHECK(handshake(device, peer));
  return GlowSecureTestAccess::replayMax(device.service(), peer.mac().data());
}

}  // namespace

GLOW_TEST(proof_counter_becomes_the_replay_max) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);
  // HELLO used 101, PROOF used 102.
  CHECK_EQ(floor, static_cast<uint64_t>(102));
}

// Everything up to and including the proof counter must count as consumed.
GLOW_TEST(proof_counter_consumes_the_whole_window_below_it) {
  Device device;
  Peer peer = makePeer();
  establishAtHighCounter(device, peer);
  CHECK_EQ(GlowSecureTestAccess::replayBitmap(device.service(), peer.mac().data()),
           ~static_cast<uint64_t>(0));
}

GLOW_TEST(fresh_data_is_delivered) {
  Device device;
  Peer peer = makePeer();
  establishAtHighCounter(device, peer);

  device.messages.clear();
  device.deliver(peer.build(Frame::DATA, toBytes(syncMessage("fresh"))));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
  CHECK_EQ(device.messages[0].first, peer.nodeId());
  CHECK(device.messages[0].second.find("fresh") != std::string::npos);
}

GLOW_TEST(duplicate_data_frame_is_dropped) {
  Device device;
  Peer peer = makePeer();
  establishAtHighCounter(device, peer);

  Bytes frame = peer.build(Frame::DATA, toBytes(syncMessage("once")));
  device.messages.clear();
  device.deliver(frame);
  device.pump();
  device.deliver(frame);
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
}

// The scenario from the design notes: a recording made just before the peer
// authenticated must not be replayable afterwards.
GLOW_TEST(counter_below_the_proof_floor_is_rejected) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);

  device.messages.clear();
  for (uint64_t counter : {floor - 1, floor - 5, floor - 63}) {
    device.deliver(peer.buildWithCounter(counter, Frame::DATA,
                                         toBytes(syncMessage("replayed"))));
    device.pump();
  }

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

GLOW_TEST(counter_far_below_the_window_is_rejected) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);

  device.messages.clear();
  device.deliver(
      peer.buildWithCounter(floor - 90, Frame::DATA, toBytes(syncMessage("ancient"))));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

GLOW_TEST(replay_window_slides_forward) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);

  device.deliver(peer.buildWithCounter(floor + 70, Frame::HEARTBEAT, {}));
  device.pump();
  CHECK_EQ(GlowSecureTestAccess::replayMax(device.service(), peer.mac().data()),
           floor + 70);

  // Now more than 64 behind the maximum: outside the window, must be refused.
  device.messages.clear();
  device.deliver(
      peer.buildWithCounter(floor + 3, Frame::DATA, toBytes(syncMessage("stale"))));
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// A gap in the counters (lost frames) must not stall the session.
GLOW_TEST(counter_gaps_are_tolerated) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);

  device.messages.clear();
  device.deliver(peer.buildWithCounter(floor + 40, Frame::DATA, toBytes(syncMessage("a"))));
  device.pump();
  device.deliver(peer.buildWithCounter(floor + 41, Frame::DATA, toBytes(syncMessage("b"))));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(2));
}

// After a receiver reboot the peer re-authenticates, and traffic captured
// before the reboot must still be rejected.
GLOW_TEST(recording_from_before_a_receiver_reboot_is_rejected) {
  Device device;
  Peer peer = makePeer();
  establishAtHighCounter(device, peer, 200);

  Bytes recorded = peer.build(Frame::DATA, toBytes(syncMessage("captured")));
  device.messages.clear();
  device.deliver(recorded);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));

  device.reboot();
  CHECK(handshake(device, peer));

  device.messages.clear();
  device.deliver(recorded);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

// The handshake frames themselves must not be replayable either.
GLOW_TEST(replayed_hello_does_not_reset_an_established_session) {
  Device device;
  Peer peer = makePeer();
  establishAtHighCounter(device, peer);

  Bytes hello = peer.buildWithCounter(101, Frame::HELLO, {});
  for (int i = 0; i < 5; ++i) {
    device.deliver(hello);
    device.pump();
  }

  CHECK(device.knows(peer.nodeId()));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(1));
}

// An old proof must not be able to reset the replay floor downwards.
GLOW_TEST(replayed_proof_does_not_lower_the_replay_floor) {
  Device device;
  Peer peer = makePeer();
  uint64_t floor = establishAtHighCounter(device, peer);

  device.deliver(peer.buildWithCounter(floor + 20, Frame::HEARTBEAT, {}));
  device.pump();

  Bytes challenge;
  CHECK(findChallengeFor(device, peer, &challenge));
  device.deliver(peer.buildWithCounter(floor, Frame::PROOF, challenge));
  device.pump();

  CHECK_EQ(GlowSecureTestAccess::replayMax(device.service(), peer.mac().data()),
           floor + 20);
}
