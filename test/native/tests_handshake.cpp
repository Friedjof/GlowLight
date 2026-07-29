// Challenge/proof handshake and session lifecycle.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

const char* kPeerBootA = "0f0e0d0c0b0a09080706050403020100";
const char* kPeerBootB = "112233445566778899aabbccddeeff00";

Peer makePeer(const char* macHex = "aabbccddee01", const char* bootHex = kPeerBootA) {
  return Peer(macHex, bootHex, testGroupKey());
}

std::string syncMessage(size_t padding = 0) {
  std::string message = "{\"type\":1,\"message\":{\"timestamp\":42,\"pad\":\"";
  message.append(padding, 'p');
  message += "\"}}";
  return message;
}

Bytes toBytes(const std::string& text) {
  return Bytes(text.begin(), text.end());
}

}  // namespace

GLOW_TEST(hello_triggers_a_challenge_bound_to_our_identity) {
  Device device;
  std::array<uint8_t, kBootIdSize> localBootId = device.bootId();
  Peer peer = makePeer();

  device.clearOutbox();
  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();

  std::vector<ParsedFrame> challenges = device.outbox(Frame::CHALLENGE);
  CHECK_EQ(challenges.size(), static_cast<size_t>(1));

  const ParsedFrame& challenge = challenges[0];
  // The challenge is sent under our identity...
  CHECK(memcmp(challenge.mac.data(), device.mac().data(), kMacSize) == 0);
  CHECK(memcmp(challenge.bootId.data(), localBootId.data(), kBootIdSize) == 0);
  // ...and names the peer it is meant for, plus 128 fresh bits.
  CHECK_EQ(challenge.plaintext.size(), kMacSize + kBootIdSize + kChallengeSize);
  CHECK(memcmp(challenge.plaintext.data(), peer.mac().data(), kMacSize) == 0);
  CHECK(memcmp(challenge.plaintext.data() + kMacSize, peer.bootId().data(), kBootIdSize) ==
        0);
}

GLOW_TEST(two_peers_get_different_challenges) {
  Device device;
  Peer first = makePeer("aabbccddee01");
  Peer second = makePeer("aabbccddee02");

  device.deliver(first.build(Frame::HELLO, {}));
  device.deliver(second.build(Frame::HELLO, {}));
  device.pump();

  Bytes firstChallenge;
  Bytes secondChallenge;
  CHECK(findChallengeFor(device, first, &firstChallenge));
  CHECK(findChallengeFor(device, second, &secondChallenge));
  CHECK(firstChallenge != secondChallenge);
}

GLOW_TEST(valid_proof_establishes_the_session_once) {
  Device device;
  Peer peer = makePeer();

  CHECK(handshake(device, peer));
  CHECK_EQ(device.connections.size(), static_cast<size_t>(1));
  CHECK_EQ(device.connections[0], peer.nodeId());
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(1));

  // Repeating HELLO/heartbeat traffic must not announce the peer again.
  device.deliver(peer.build(Frame::HELLO, {}));
  device.deliver(peer.build(Frame::HEARTBEAT, {}));
  device.pump();
  CHECK_EQ(device.connections.size(), static_cast<size_t>(1));
}

GLOW_TEST(wrong_proof_does_not_establish) {
  Device device;
  Peer peer = makePeer();

  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();
  device.deliver(peer.build(Frame::PROOF, Bytes(kChallengeSize, 0x00)));
  device.pump();

  CHECK(!device.knows(peer.nodeId()));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(0));
}

GLOW_TEST(proof_of_the_wrong_length_does_not_establish) {
  Device device;
  Peer peer = makePeer();

  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();
  Bytes challenge;
  CHECK(findChallengeFor(device, peer, &challenge));

  challenge.pop_back();
  device.deliver(peer.build(Frame::PROOF, challenge));
  device.pump();
  CHECK(!device.knows(peer.nodeId()));
}

GLOW_TEST(expired_challenge_is_not_accepted) {
  Device device;
  Peer peer = makePeer();

  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();
  Bytes challenge;
  CHECK(findChallengeFor(device, peer, &challenge));

  // The challenge lifetime is five seconds.
  device.advance(6000);
  device.deliver(peer.build(Frame::PROOF, challenge));
  device.pump();

  CHECK(!device.knows(peer.nodeId()));
}

// A proof nobody asked for must never establish a session.
GLOW_TEST(unsolicited_proof_does_not_establish) {
  Device device;
  Peer peer = makePeer();

  device.deliver(peer.build(Frame::PROOF, Bytes(kChallengeSize, 0x11)));
  device.pump();
  CHECK(!device.knows(peer.nodeId()));
}

// We only answer a challenge that names our current MAC and boot id.
GLOW_TEST(challenge_for_a_foreign_identity_gets_no_proof) {
  Device device;
  Peer peer = makePeer();

  Bytes payload;
  Bytes foreignMac = fromHex("001122334455");
  payload.insert(payload.end(), foreignMac.begin(), foreignMac.end());
  payload.insert(payload.end(), kBootIdSize, 0x00);
  payload.insert(payload.end(), kChallengeSize, 0x77);

  device.clearOutbox();
  device.deliver(peer.build(Frame::CHALLENGE, payload));
  device.pump();
  CHECK(device.outbox(Frame::PROOF).empty());
}

GLOW_TEST(challenge_for_a_stale_boot_id_gets_no_proof) {
  Device device;
  Peer peer = makePeer();

  Bytes payload;
  payload.insert(payload.end(), device.mac().begin(), device.mac().end());
  payload.insert(payload.end(), kBootIdSize, 0xab);  // not our boot id
  payload.insert(payload.end(), kChallengeSize, 0x77);

  device.clearOutbox();
  device.deliver(peer.build(Frame::CHALLENGE, payload));
  device.pump();
  CHECK(device.outbox(Frame::PROOF).empty());
}

GLOW_TEST(challenge_naming_us_is_answered_with_a_proof) {
  Device device;
  std::array<uint8_t, kBootIdSize> localBootId = device.bootId();
  Peer peer = makePeer();

  Bytes challenge(kChallengeSize, 0x5a);
  Bytes payload;
  payload.insert(payload.end(), device.mac().begin(), device.mac().end());
  payload.insert(payload.end(), localBootId.begin(), localBootId.end());
  payload.insert(payload.end(), challenge.begin(), challenge.end());

  device.clearOutbox();
  device.deliver(peer.build(Frame::CHALLENGE, payload));
  device.pump();

  std::vector<ParsedFrame> proofs = device.outbox(Frame::PROOF);
  CHECK_EQ(proofs.size(), static_cast<size_t>(1));
  CHECK(proofs[0].plaintext == challenge);
}

// A replayed challenge must not buy an attacker unlimited proof transmissions.
GLOW_TEST(replayed_challenge_does_not_amplify_proofs) {
  Device device;
  std::array<uint8_t, kBootIdSize> localBootId = device.bootId();
  Peer peer = makePeer();

  Bytes payload;
  payload.insert(payload.end(), device.mac().begin(), device.mac().end());
  payload.insert(payload.end(), localBootId.begin(), localBootId.end());
  payload.insert(payload.end(), kChallengeSize, 0x5a);
  Bytes frame = peer.build(Frame::CHALLENGE, payload);

  device.clearOutbox();
  for (int i = 0; i < 10; ++i) {
    device.deliver(frame);
    device.pump(2);
  }

  // The first proof is legitimate; the nine replays must be dropped.
  CHECK_EQ(device.outbox(Frame::PROOF).size(), static_cast<size_t>(1));
}

// Recordings of many different old boot sessions must not turn into a
// challenge storm either.
GLOW_TEST(replays_of_many_old_boot_sessions_are_rate_limited) {
  Device device;
  device.clearOutbox();

  for (int i = 0; i < 20; ++i) {
    char bootId[33];
    snprintf(bootId, sizeof(bootId), "%032x", i + 1);
    Peer stale = makePeer("aabbccddee01", bootId);
    device.deliver(stale.build(Frame::HELLO, {}));
    device.pump(2);
  }

  // Without any time passing only the first replay may produce a challenge.
  CHECK_EQ(device.outbox(Frame::CHALLENGE).size(), static_cast<size_t>(1));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(0));
}

// ------------------------------------------------------- session lifecycle ---

// A recording from an earlier boot of a peer must not evict its live session.
GLOW_TEST(replayed_old_boot_frame_does_not_drop_an_established_peer) {
  Device device;
  Peer peer = makePeer("aabbccddee01", kPeerBootB);
  peer.setCounter(50);
  CHECK(handshake(device, peer));

  // Attacker replays a frame captured during a previous boot of the same lamp.
  Peer previousBoot = makePeer("aabbccddee01", kPeerBootA);
  Bytes recorded = previousBoot.build(Frame::HELLO, {});
  for (int i = 0; i < 3; ++i) {
    device.deliver(recorded);
    device.pump();
  }

  CHECK(device.knows(peer.nodeId()));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(1));

  // The live peer must still be able to deliver data.
  device.messages.clear();
  for (const Bytes& fragment : peer.buildData(syncMessage())) device.deliver(fragment);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
}

// A genuine reboot of a peer renegotiates and keeps working.
GLOW_TEST(peer_reboot_negotiates_a_new_session) {
  Device device;
  Peer peer = makePeer("aabbccddee01", kPeerBootA);
  CHECK(handshake(device, peer));

  peer.setBootId(kPeerBootB);
  CHECK(handshake(device, peer));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(1));

  device.messages.clear();
  for (const Bytes& fragment : peer.buildData(syncMessage())) device.deliver(fragment);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(1));
}

// After our own reboot every peer must prove itself again before it is trusted.
GLOW_TEST(receiver_reboot_requires_a_fresh_proof) {
  Device device;
  Peer peer = makePeer();
  peer.setCounter(20);
  CHECK(handshake(device, peer));

  device.reboot();
  CHECK(!device.knows(peer.nodeId()));

  // Data alone is not enough after the reboot.
  device.messages.clear();
  for (const Bytes& fragment : peer.buildData(syncMessage())) device.deliver(fragment);
  device.pump();
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
  CHECK(!device.knows(peer.nodeId()));

  CHECK(handshake(device, peer));
}

GLOW_TEST(eight_peers_fit_into_the_group) {
  Device device;
  for (int i = 0; i < 8; ++i) {
    char mac[16];
    snprintf(mac, sizeof(mac), "aabbccddee%02x", i + 1);
    Peer peer = makePeer(mac);
    CHECK(handshake(device, peer));
  }
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(8));
  CHECK_EQ(GlowSecureTestAccess::establishedSessions(device.service()),
           static_cast<size_t>(8));
}

GLOW_TEST(ninth_peer_is_refused_while_the_group_is_full) {
  Device device;
  for (int i = 0; i < 8; ++i) {
    char mac[16];
    snprintf(mac, sizeof(mac), "aabbccddee%02x", i + 1);
    Peer peer = makePeer(mac);
    CHECK(handshake(device, peer));
  }

  Peer extra = makePeer("aabbccddeeff");
  CHECK(!handshake(device, extra));
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(8));
}

// Session slots must not leak: once a peer has been gone longer than the node
// timeout its slot has to become available again.
GLOW_TEST(idle_sessions_are_reclaimed_so_new_peers_can_join) {
  Device device;
  for (int i = 0; i < 8; ++i) {
    char mac[16];
    snprintf(mac, sizeof(mac), "aabbccddee%02x", i + 1);
    Peer peer = makePeer(mac);
    CHECK(handshake(device, peer));
  }

  // All eight lamps disappear and stay silent past the node timeout.
  device.advance(GLOW_NODE_TIMEOUT + 60000);
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(0));

  Peer newcomer = makePeer("aabbccddeeff");
  CHECK(handshake(device, newcomer));
}
