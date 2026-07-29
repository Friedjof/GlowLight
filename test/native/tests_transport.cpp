// Send path, transmit queue behaviour and peer discovery bookkeeping.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

Peer makePeer(const char* macHex = "aabbccddee01") {
  return Peer(macHex, "0f0e0d0c0b0a09080706050403020100", testGroupKey());
}

std::vector<uint64_t> outboundCounters(Device& device) {
  std::vector<uint64_t> counters;
  for (const ParsedFrame& frame : device.outbox()) counters.push_back(frame.counter);
  return counters;
}

}  // namespace

// Other services name this lamp by its node id. With the transport switched
// off it used to report zero, so every lamp looked like the same device.
GLOW_TEST(the_node_id_exists_even_with_the_transport_disabled) {
  glow_shim::resetEspNow();
  glow_shim::clockMillis = 0;
  memcpy(glow_shim::localMac, fromHex("aabbccddee01").data(), 6);

  CommunicationService service;
  CommunicationConfig config;
  config.enabled = false;
  service.setup(config);

  CHECK(!service.isAvailable());
  CHECK_EQ(service.getNodeId(), Peer::nodeIdFor(fromHex("aabbccddee01").data()));
  CHECK(service.getNodeId() != 0);
}

GLOW_TEST(node_id_is_derived_from_the_mac) {
  Device device;
  CHECK_EQ(device.service().getNodeId(), Peer::nodeIdFor(device.mac().data()));
  CHECK_EQ(device.nodeCount(), static_cast<size_t>(0));
}

// Every transmitted frame must use a fresh counter: reuse would repeat a nonce.
GLOW_TEST(counters_never_repeat_across_the_send_api) {
  Device device;
  CommunicationService& service = device.service();

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  service.sendStateEvent(state);
  device.pump(4);

  JsonDocument payload;
  payload["speed"] = 7;
  service.sendModeCommand("rainbow", "Rainbow", "1.0", 1, "setSpeed", payload);
  device.pump(4);

  service.sendSyncRequest();
  device.pump(4);
  service.sendWipe(3);
  device.pump(4);
  service.sendDistanceUpdate(120, 200);
  device.pump(4);

  std::vector<uint64_t> counters = outboundCounters(device);
  CHECK(counters.size() >= 6);  // boot HELLO plus the five messages
  for (size_t i = 1; i < counters.size(); ++i) CHECK(counters[i] > counters[i - 1]);
}

// Mode commands drive cross-lamp features (sunset start, strobe sync, ...).
// A valid command must actually reach the air.
GLOW_TEST(mode_command_with_an_object_payload_is_broadcast) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  JsonDocument payload;
  payload["speed"] = 7;
  device.service().sendModeCommand("strobe", "Strobe", "1.0", 1, "speed_change", payload);
  device.pump(4);

  std::vector<ParsedFrame> frames = device.outbox(Frame::DATA);
  CHECK(!frames.empty());

  std::string sent;
  for (const ParsedFrame& frame : frames) {
    sent.append(frame.plaintext.begin(), frame.plaintext.end());
  }
  CHECK(sent.find("\"command\":\"speed_change\"") != std::string::npos);
  CHECK(sent.find("\"id\":\"strobe\"") != std::string::npos);
  CHECK(sent.find("\"schemaVersion\":1") != std::string::npos);
  CHECK(sent.find("\"sync\":{\"revision\":1") != std::string::npos);
  CHECK(sent.find("\"speed\":7") != std::string::npos);
  CHECK(sent.find("\"title\":\"Strobe\"") != std::string::npos);
}

GLOW_TEST(application_publish_gate_does_not_disable_sync_requests) {
  Device device;
  device.pump(2);
  device.clearOutbox();
  device.service().setApplicationPublishing(false);

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  device.service().sendStateEvent(state);
  device.service().sendWipe(2);
  device.service().sendDistanceUpdate(100, 200);
  device.pump(8);
  CHECK(device.outbox(Frame::DATA).empty());

  device.service().sendSyncRequest();
  device.pump(4);
  std::vector<ParsedFrame> frames = device.outbox(Frame::DATA);
  CHECK_EQ(frames.size(), static_cast<size_t>(1));
  std::string sent(frames[0].plaintext.begin(), frames[0].plaintext.end());
  CHECK(sent.find("\"kind\":\"state.request\"") != std::string::npos);
}

GLOW_TEST(application_frames_queued_before_gate_closure_are_dropped) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  CHECK(device.service().sendStateEvent(state));
  CHECK(device.service().setApplicationPublishing(false));
  device.pump(4);

  CHECK(device.outbox(Frame::DATA).empty());
}

GLOW_TEST(state_snapshots_preserve_the_current_version_and_bypass_the_gate) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  device.service().sendStateEvent(state);
  device.pump(6);
  SyncVersion published = device.service().syncVersion();
  device.clearOutbox();

  device.service().setApplicationPublishing(false);
  device.service().sendStateSnapshot(state, 1234);
  device.pump(6);
  std::string sent;
  for (const ParsedFrame& frame : device.outbox(Frame::DATA)) {
    sent.append(frame.plaintext.begin(), frame.plaintext.end());
  }
  CHECK(sent.find("\"revision\":" + std::to_string(published.revision)) !=
        std::string::npos);
  CHECK(sent.find("\"origin\":" + std::to_string(published.origin)) !=
        std::string::npos);
  CHECK(sent.find("\"replyTo\":1234") != std::string::npos);
}

GLOW_TEST(mode_command_without_a_command_name_is_not_sent) {
  Device device;
  device.pump(2);
  size_t before = device.rawOutboxSize();

  JsonDocument payload;
  payload["speed"] = 7;
  device.service().sendModeCommand("strobe", "Strobe", "1.0", 1, "", payload);
  device.pump(4);

  CHECK_EQ(device.rawOutboxSize(), before);
}

GLOW_TEST(mode_state_without_a_registry_is_not_sent) {
  Device device;
  device.pump(2);
  size_t before = device.rawOutboxSize();

  JsonDocument state;
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  device.service().sendStateEvent(state);
  device.pump(4);

  CHECK_EQ(device.rawOutboxSize(), before);
}

GLOW_TEST(message_larger_than_the_plaintext_limit_is_refused) {
  Device device;
  device.pump(2);
  size_t before = device.rawOutboxSize();

  JsonDocument payload;
  payload["blob"] = std::string(600, 'x');
  device.service().sendModeCommand("rainbow", "Rainbow", "1.0", 1, "setSpeed", payload);
  device.pump(6);

  CHECK_EQ(device.rawOutboxSize(), before);
}

GLOW_TEST(a_full_transmit_queue_drops_frames_without_stalling) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  // The queue holds 16 frames; everything beyond that is dropped.
  for (int i = 0; i < 24; ++i) device.service().sendSyncRequest();
  device.pump(40);

  CHECK_EQ(device.rawOutboxSize(), static_cast<size_t>(16));

  // The node must keep working afterwards.
  device.clearOutbox();
  device.service().sendSyncRequest();
  device.pump(4);
  CHECK_EQ(device.rawOutboxSize(), static_cast<size_t>(1));
}

GLOW_TEST(a_dropped_state_event_does_not_advance_the_sync_version) {
  Device device;
  device.pump(2);
  device.clearOutbox();
  for (int i = 0; i < 16; ++i) device.service().sendSyncRequest();

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  CHECK(!device.service().sendStateEvent(state));
  CHECK_EQ(device.service().syncVersion().revision, static_cast<uint64_t>(0));
}

// The sync clock is a logical clock: every local change publishes one above the
// highest revision seen. A peer that reports an absurd revision must not be able
// to push the clock so high that the next increment wraps to zero — from then on
// this lamp would lose every comparison and only a reboot of the whole group
// would recover it.
GLOW_TEST(an_absurd_remote_revision_cannot_wrap_the_sync_clock) {
  Device device;
  device.pump(2);

  device.service().acceptSyncVersion(SyncVersion(UINT64_MAX, 4242));

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  CHECK(device.service().sendStateEvent(state));
  device.pump(6);

  // Whatever the lamp publishes next has to stay a usable, non-zero revision.
  SyncVersion published = device.service().syncVersion();
  CHECK(published.revision != 0);
  CHECK_EQ(published.origin, device.nodeId());
}

// Plausible jumps must still be adopted, otherwise a lamp that was offline for a
// while could never catch up with the group.
GLOW_TEST(a_plausible_remote_revision_is_still_adopted) {
  Device device;
  device.pump(2);

  device.service().acceptSyncVersion(SyncVersion(5000, 4242));

  JsonDocument state;
  state["id"] = "rainbow";
  state["title"] = "Rainbow";
  state["version"] = "1.0";
  state["schemaVersion"] = 1;
  state["registry"]["speed"] = 4;
  CHECK(device.service().sendStateEvent(state));
  device.pump(6);

  CHECK_EQ(device.service().syncVersion().revision, static_cast<uint64_t>(5001));
}

// A missing send-complete callback must not silence the node forever.
GLOW_TEST(a_lost_send_callback_recovers_after_a_timeout) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  glow_shim::suppressSendCallback = true;
  device.service().sendSyncRequest();
  device.service().sendSyncRequest();
  device.pump(10);
  CHECK_EQ(device.rawOutboxSize(), static_cast<size_t>(1));

  glow_shim::suppressSendCallback = false;
  device.advance(500, 6);
  CHECK(device.rawOutboxSize() >= 2);
}

GLOW_TEST(a_rejected_send_does_not_stall_the_queue) {
  Device device;
  device.pump(2);
  device.clearOutbox();

  glow_shim::sendShouldFail = true;
  device.service().sendSyncRequest();
  device.pump(4);
  CHECK_EQ(device.rawOutboxSize(), static_cast<size_t>(0));

  glow_shim::sendShouldFail = false;
  device.service().sendSyncRequest();
  device.pump(4);
  CHECK(device.rawOutboxSize() >= 1);
}

// Heartbeats are emitted on schedule and carry no payload.
GLOW_TEST(heartbeats_are_emitted_periodically) {
  Device device;
  device.pump(4);
  device.clearOutbox();

  device.advance(HARTBEAT_INTERVAL + 100, 6);

  std::vector<ParsedFrame> heartbeats = device.outbox(Frame::HEARTBEAT);
  CHECK_EQ(heartbeats.size(), static_cast<size_t>(1));
  CHECK_EQ(heartbeats[0].plaintext.size(), static_cast<size_t>(0));
  CHECK(!device.outbox(Frame::HELLO).empty());
}

// ------------------------------------------------------------- discovery ----

GLOW_TEST(unauthenticated_traffic_never_creates_a_node) {
  Device device;
  Peer peer = makePeer();

  device.deliver(peer.build(Frame::HELLO, {}));
  device.deliver(peer.build(Frame::HEARTBEAT, {}));
  device.deliver(peer.build(Frame::DATA, Bytes(40, 'x')));
  device.pump();

  CHECK_EQ(device.nodeCount(), static_cast<size_t>(0));
  CHECK_EQ(device.connections.size(), static_cast<size_t>(0));
  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

GLOW_TEST(a_node_is_forgotten_after_the_timeout) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));
  CHECK(device.knows(peer.nodeId()));

  device.advance(GLOW_NODE_TIMEOUT + 60000);
  CHECK(!device.knows(peer.nodeId()));
}

GLOW_TEST(heartbeats_keep_a_node_alive) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));

  for (int i = 0; i < 8; ++i) {
    device.advance(5 * 60 * 1000);
    device.deliver(peer.build(Frame::HEARTBEAT, {}));
    device.pump();
  }

  CHECK(device.knows(peer.nodeId()));
}

// A dropped frame must not refresh the peer's last-seen timestamp.
GLOW_TEST(rejected_frames_do_not_refresh_a_node) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));

  Bytes tampered = peer.build(Frame::HEARTBEAT, {});
  tampered[tampered.size() - 1] ^= 0x01;

  for (int i = 0; i < 8; ++i) {
    device.advance(5 * 60 * 1000);
    device.deliver(tampered);
    device.pump();
  }

  CHECK(!device.knows(peer.nodeId()));
}

// The application never sees the transport's own heartbeat messages.
GLOW_TEST(heartbeat_messages_are_not_forwarded_to_the_application) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));
  device.messages.clear();

  std::string heartbeat = "{\"type\":2,\"message\":{}}";
  device.deliver(peer.build(Frame::DATA, Bytes(heartbeat.begin(), heartbeat.end())));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

GLOW_TEST(malformed_json_from_an_authenticated_peer_is_ignored) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));
  device.messages.clear();

  std::string garbage = "not json at all";
  device.deliver(peer.build(Frame::DATA, Bytes(garbage.begin(), garbage.end())));

  std::string outOfRange = "{\"type\":99,\"message\":{}}";
  device.deliver(peer.build(Frame::DATA, Bytes(outOfRange.begin(), outOfRange.end())));

  std::string negative = "{\"type\":-1,\"message\":{}}";
  device.deliver(peer.build(Frame::DATA, Bytes(negative.begin(), negative.end())));
  device.pump();

  CHECK_EQ(device.messages.size(), static_cast<size_t>(0));
}

GLOW_TEST(application_receives_the_message_type) {
  Device device;
  Peer peer = makePeer();
  CHECK(handshake(device, peer));
  device.messages.clear();
  device.messageTypes.clear();

  std::string wipe = "{\"type\":3,\"message\":{\"numberOfWipes\":2}}";
  device.deliver(peer.build(Frame::DATA, Bytes(wipe.begin(), wipe.end())));
  device.pump();

  CHECK_EQ(device.messageTypes.size(), static_cast<size_t>(1));
  CHECK(device.messageTypes[0] == MessageType::WIPE);
  CHECK_EQ(device.messages[0].second, std::string("{\"numberOfWipes\":2}"));
}
