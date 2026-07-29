#include "Controller.h"
#include "support.h"

using namespace glowtest;

struct GlowControllerTestAccess {
  static void receive(Controller& controller, uint32_t from, JsonDocument message,
                      MessageType type) {
    controller.newMessageCallback(from, message, type);
  }

  static void maintain(Controller& controller) { controller.maintainSync(); }
};

namespace {

class FakeMode : public AbstractMode {
 public:
  FakeMode(const char* modeId, const char* modeTitle, LightService* light,
           DistanceService* distance, CommunicationService* communication)
      : AbstractMode(light, distance, communication) {
    this->id = modeId;
    this->title = modeTitle;
    this->description = "Test mode";
    this->version = "1.0.0";
  }

  void setup() override {
    this->registry.init("speed", RegistryType::INT, 2, 1, 4);
    this->registry.init("running", RegistryType::BOOL, false);
    this->addOption("Speed", []() {}, false);
    JsonDocument arguments;
    arguments.to<JsonObject>();
    this->declareCommand("toggle", arguments);
  }

  void customFirst() override { ++this->activations; }
  void customLoop() override {}
  void last() override {}
  void customClick() override {}

  bool handleRemoteCommand(const String& command,
                           const JsonDocument& payload) override {
    if (command != "toggle") return false;
    this->registry.setBool("running", !this->registry.getBool("running"));
    return true;
  }

  int activations = 0;
};

// A mode built like Static Light: its options are presets that write their result
// into the registry. That is the shape that made a manually set colour bounce back.
class PresetMode : public AbstractMode {
 public:
  PresetMode(LightService* light, DistanceService* distance,
             CommunicationService* communication)
      : AbstractMode(light, distance, communication) {
    this->id = "preset";
    this->title = "Preset";
    this->description = "Presets that write settings";
    this->version = "1.0.0";
  }

  void setup() override {
    this->registry.init("color", RegistryType::COLOR, CRGB(255, 128, 20));
    this->addOption("Warm", [this]() { this->apply(CRGB(255, 128, 20)); }, false, true);
    this->addOption("Red", [this]() { this->apply(CRGB(240, 70, 70)); }, false, true);
  }

  void customFirst() override {}
  void customLoop() override {}
  void last() override {}
  void customClick() override {}

  int applications = 0;

 private:
  void apply(CRGB color) {
    ++this->applications;
    this->registry.setColor("color", color);
  }
};

JsonDocument request(const char* operation, const char* mode = nullptr) {
  JsonDocument document;
  document["api"] = "glow.control/1";
  document["requestId"] = "test-1";
  document["operation"] = operation;
  if (mode != nullptr) document["target"]["mode"] = mode;
  return document;
}

JsonDocument syncPolicy(bool follow, bool publish) {
  JsonDocument document = request("sync.configure");
  document["scope"] = "local";
  document["sync"]["follow"] = follow;
  document["sync"]["publish"] = publish;
  return document;
}

JsonDocument stateEvent(int speed, uint64_t revision, uint32_t origin,
                        uint32_t replyTo = 0) {
  JsonDocument message;
  message["eventKind"] = static_cast<uint8_t>(ModeEventKind::STATE);
  message["mode"]["id"] = "first";
  message["mode"]["title"] = "First";
  message["mode"]["version"] = "1.0.0";
  message["mode"]["schemaVersion"] = 1;
  message["sync"]["revision"] = revision;
  message["sync"]["origin"] = origin;
  if (replyTo != 0) message["sync"]["replyTo"] = replyTo;
  message["payload"]["speed"] = speed;
  message["payload"]["running"] = false;
  message["payload"]["currentOption"] = 0;
  message["payload"]["brightness"] = LED_DEFAULT_BRIGHTNESS;
  return message;
}

struct Fixture {
  LightService light;
  DistanceService distance;
  CommunicationService communication;
  Controller controller{&distance, &communication};
  FakeMode first{"first", "First", &light, &distance, &communication};
  FakeMode second{"second", "Second", &light, &distance, &communication};

  Fixture() {
    controller.addMode(&first);
    controller.addMode(&second);
  }

  void startTransport() {
    glow_shim::resetEspNow();
    glow_shim::clockMillis = 0;
    communication.setup();
    controller.requestResync();
  }
};

}  // namespace

// The bug: the preset ran again on the next loop and overwrote what was just set.
GLOW_TEST(a_preset_does_not_overwrite_a_setting_changed_afterwards) {
  Fixture fixture;
  PresetMode preset{&fixture.light, &fixture.distance, &fixture.communication};
  fixture.controller.addMode(&preset);
  fixture.controller.executeControl(request("mode.select", "preset"));

  JsonDocument set = request("mode.setting.set", "preset");
  set["setting"] = "color";
  set["value"] = "0000FF";
  CHECK(fixture.controller.executeControl(set)["ok"].as<bool>());

  for (int iteration = 0; iteration < 5; ++iteration) preset.loop();
  CHECK(fixture.controller.state()["mode"]["registry"]["color"].as<String>() == "0000FF");
}

// The same overwrite on the receiving side: the synced state is authoritative, so
// re-running the preset would undo exactly what arrived.
GLOW_TEST(applying_group_state_does_not_rerun_a_preset) {
  Fixture fixture;
  PresetMode preset{&fixture.light, &fixture.distance, &fixture.communication};
  fixture.controller.addMode(&preset);
  fixture.controller.executeControl(request("mode.select", "preset"));

  JsonDocument state;
  state["id"] = "preset";
  state["schemaVersion"] = 1;
  state["registry"]["color"] = "0000FF";
  state["registry"]["currentOption"] = 0;
  state["registry"]["brightness"] = LED_DEFAULT_BRIGHTNESS;
  CHECK(preset.deserialize(state));

  for (int iteration = 0; iteration < 5; ++iteration) preset.loop();
  CHECK(fixture.controller.state()["mode"]["registry"]["color"].as<String>() == "0000FF");
}

// Selecting a preset must have taken effect by the time the state is published,
// or the group would receive the values the preset is about to replace.
GLOW_TEST(a_selected_preset_is_applied_before_the_state_is_published) {
  Fixture fixture;
  PresetMode preset{&fixture.light, &fixture.distance, &fixture.communication};
  fixture.controller.addMode(&preset);
  fixture.controller.executeControl(request("mode.select", "preset"));

  preset.applications = 0;
  JsonDocument select = request("mode.option.set", "preset");
  select["scope"] = "local";
  select["option"] = 1;
  CHECK(fixture.controller.executeControl(select)["ok"].as<bool>());
  CHECK(fixture.controller.state()["mode"]["registry"]["color"].as<String>() == "F04646");
  CHECK_EQ(preset.applications, 1);
}

GLOW_TEST(capabilities_are_derived_from_registered_modes) {
  Fixture fixture;
  JsonDocument capabilities = fixture.controller.capabilities();

  CHECK(capabilities["schema"].as<String>() == "glow.capabilities");
  CHECK_EQ(capabilities["schemaVersion"].as<int>(), 1);
  CHECK_EQ(capabilities["modes"].size(), static_cast<size_t>(2));
  CHECK(capabilities["modes"][0]["id"].as<String>() == "first");
  CHECK(capabilities["modes"][1]["id"].as<String>() == "second");
  CHECK(capabilities["modes"][0]["settings"]["speed"]["type"].as<String>() ==
        "integer");
  CHECK(capabilities["modes"][0]["commands"]["toggle"].is<JsonObject>());
  CHECK(capabilities["features"]["syncPolicy"]["supported"].as<bool>());
  CHECK(capabilities["features"]["syncPolicy"]["defaults"]["follow"].as<bool>());
  CHECK(capabilities["features"]["syncPolicy"]["defaults"]["publish"].as<bool>());
}

GLOW_TEST(control_selects_modes_and_reflects_request_ids) {
  Fixture fixture;
  JsonDocument select = request("mode.select", "first");
  JsonDocument response = fixture.controller.executeControl(select);

  CHECK(response["ok"].as<bool>());
  CHECK(response["requestId"].as<String>() == "test-1");
  CHECK(fixture.controller.getCurrentModeId() == "first");
  CHECK_EQ(fixture.first.activations, 1);
}

GLOW_TEST(control_validates_and_applies_settings_atomically) {
  Fixture fixture;
  fixture.controller.executeControl(request("mode.select", "first"));

  JsonDocument set = request("mode.setting.set", "first");
  set["setting"] = "speed";
  set["value"] = 4;
  CHECK(fixture.controller.executeControl(set)["ok"].as<bool>());
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 4);

  set["value"] = 5;
  JsonDocument rejected = fixture.controller.executeControl(set);
  CHECK(!rejected["ok"].as<bool>());
  CHECK(rejected["error"]["code"].as<String>() == "INVALID_SETTING");
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 4);
}

GLOW_TEST(control_dispatches_options_and_declared_commands) {
  Fixture fixture;
  fixture.controller.executeControl(request("mode.select", "first"));

  JsonDocument option = request("mode.option.set", "first");
  option["option"] = 0;
  CHECK(fixture.controller.executeControl(option)["ok"].as<bool>());

  JsonDocument command = request("mode.command", "first");
  command["command"] = "toggle";
  command["arguments"].to<JsonObject>();
  CHECK(fixture.controller.executeControl(command)["ok"].as<bool>());
  CHECK(fixture.controller.state()["mode"]["registry"]["running"].as<bool>());

  command["command"] = "unknown";
  CHECK(!fixture.controller.executeControl(command)["ok"].as<bool>());

  command["command"] = "toggle";
  command["arguments"]["unexpected"] = true;
  CHECK(!fixture.controller.executeControl(command)["ok"].as<bool>());
}

GLOW_TEST(control_rejects_unknown_or_inactive_targets) {
  Fixture fixture;
  JsonDocument unknown = request("mode.select", "missing");
  CHECK(fixture.controller.executeControl(unknown)["error"]["code"].as<String>() ==
        "UNKNOWN_MODE");

  fixture.controller.executeControl(request("mode.select", "first"));
  JsonDocument inactive = request("mode.setting.set", "second");
  inactive["setting"] = "speed";
  inactive["value"] = 3;
  CHECK(fixture.controller.executeControl(inactive)["error"]["code"].as<String>() ==
        "MODE_NOT_ACTIVE");
}

GLOW_TEST(group_control_fails_when_the_transport_is_unavailable) {
  Fixture fixture;
  JsonDocument select = request("mode.select", "first");
  select["scope"] = "group";

  JsonDocument response = fixture.controller.executeControl(select);
  CHECK(!response["ok"].as<bool>());
  CHECK(response["error"]["code"].as<String>() == "GROUP_UNAVAILABLE");
  CHECK_EQ(fixture.first.activations, 0);
}

GLOW_TEST(sync_policy_requires_two_booleans_and_local_scope) {
  Fixture fixture;
  JsonDocument invalid = request("sync.configure");
  invalid["sync"]["follow"] = false;
  CHECK(fixture.controller.executeControl(invalid)["error"]["code"].as<String>() ==
        "INVALID_SYNC_POLICY");

  JsonDocument group = syncPolicy(false, false);
  group["scope"] = "group";
  CHECK(fixture.controller.executeControl(group)["error"]["code"].as<String>() ==
        "INVALID_SCOPE");

  JsonDocument response = fixture.controller.executeControl(syncPolicy(false, false));
  CHECK(response["ok"].as<bool>());
  CHECK(!response["sync"]["follow"].as<bool>());
  CHECK(!response["sync"]["publish"].as<bool>());
}

GLOW_TEST(publish_disabled_blocks_group_scope_but_not_local_changes) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(syncPolicy(false, false));

  JsonDocument group = request("mode.select", "first");
  group["scope"] = "group";
  CHECK(fixture.controller.executeControl(group)["error"]["code"].as<String>() ==
        "SYNC_PUBLISH_DISABLED");
  CHECK_EQ(fixture.first.activations, 0);

  CHECK(fixture.controller.executeControl(request("mode.select", "first"))["ok"].as<bool>());
  JsonDocument state = fixture.controller.state();
  CHECK(state["sync"]["localDirty"].as<bool>());
  CHECK(state["sync"]["status"].as<String>() == "detached");
}

GLOW_TEST(follow_disabled_ignores_group_state_and_ties_are_deterministic) {
  Fixture fixture;
  fixture.controller.executeControl(request("mode.select", "first"));
  fixture.controller.executeControl(syncPolicy(false, false));

  GlowControllerTestAccess::receive(fixture.controller, 100, stateEvent(4, 1, 200),
                                    MessageType::EVENT);
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 2);

  fixture.controller.executeControl(syncPolicy(true, false));
  GlowControllerTestAccess::receive(fixture.controller, 100, stateEvent(4, 1, 200),
                                    MessageType::EVENT);
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 4);

  GlowControllerTestAccess::receive(fixture.controller, 101, stateEvent(3, 1, 199),
                                    MessageType::EVENT);
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 4);
  GlowControllerTestAccess::receive(fixture.controller, 102, stateEvent(3, 1, 201),
                                    MessageType::EVENT);
  CHECK_EQ(fixture.controller.state()["mode"]["registry"]["speed"].as<int>(), 3);
}

GLOW_TEST(rejoining_discards_local_dirty_state_for_an_equal_group_snapshot) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  GlowControllerTestAccess::receive(fixture.controller, 200, stateEvent(2, 5, 200),
                                    MessageType::EVENT);

  fixture.controller.executeControl(syncPolicy(false, false));
  JsonDocument local = request("mode.setting.set", "first");
  local["setting"] = "speed";
  local["value"] = 4;
  CHECK(fixture.controller.executeControl(local)["ok"].as<bool>());
  CHECK(fixture.controller.state()["sync"]["localDirty"].as<bool>());

  fixture.controller.executeControl(syncPolicy(true, false));
  GlowControllerTestAccess::receive(
      fixture.controller, 200,
      stateEvent(2, 5, 200, fixture.communication.getNodeId()), MessageType::EVENT);
  JsonDocument state = fixture.controller.state();
  CHECK_EQ(state["mode"]["registry"]["speed"].as<int>(), 2);
  CHECK(!state["sync"]["localDirty"].as<bool>());
  CHECK(state["sync"]["status"].as<String>() == "synchronized");
}

GLOW_TEST(rejoining_rejects_an_older_addressed_snapshot) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  GlowControllerTestAccess::receive(fixture.controller, 200, stateEvent(2, 5, 200),
                                    MessageType::EVENT);

  fixture.controller.executeControl(syncPolicy(false, false));
  JsonDocument local = request("mode.setting.set", "first");
  local["setting"] = "speed";
  local["value"] = 4;
  fixture.controller.executeControl(local);
  fixture.controller.executeControl(syncPolicy(true, false));

  GlowControllerTestAccess::receive(
      fixture.controller, 300,
      stateEvent(3, 4, 300, fixture.communication.getNodeId()), MessageType::EVENT);
  JsonDocument state = fixture.controller.state();
  CHECK_EQ(state["mode"]["registry"]["speed"].as<int>(), 4);
  CHECK(state["sync"]["localDirty"].as<bool>());
  CHECK(state["sync"]["status"].as<String>() == "joining");
}

GLOW_TEST(rejoining_completes_when_clean_local_state_wins_the_version_tie) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  GlowControllerTestAccess::receive(fixture.controller, 200, stateEvent(2, 5, 200),
                                    MessageType::EVENT);
  CHECK(!fixture.controller.state()["sync"]["localDirty"].as<bool>());

  fixture.controller.requestResync();
  GlowControllerTestAccess::receive(
      fixture.controller, 199,
      stateEvent(4, 5, 199, fixture.communication.getNodeId()), MessageType::EVENT);

  JsonDocument state = fixture.controller.state();
  CHECK_EQ(state["mode"]["registry"]["speed"].as<int>(), 2);
  CHECK_EQ(state["sync"]["revision"].as<uint64_t>(), static_cast<uint64_t>(5));
  CHECK_EQ(state["sync"]["origin"].as<uint32_t>(), static_cast<uint32_t>(200));
  CHECK(!state["sync"]["localDirty"].as<bool>());
  CHECK(state["sync"]["status"].as<String>() == "synchronized");
}

GLOW_TEST(a_publish_disabled_lamp_without_peers_completes_joining) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  fixture.controller.executeControl(syncPolicy(true, false));

  glow_shim::clockMillis += 2001;
  GlowControllerTestAccess::maintain(fixture.controller);

  JsonDocument state = fixture.controller.state();
  CHECK(state["sync"]["status"].as<String>() == "synchronized");
  CHECK(state["sync"]["localDirty"].as<bool>());
  CHECK(!fixture.communication.isApplicationPublishing());
}

namespace {

// A controller wired to a Device, so authenticated peers really appear in the
// node list and outgoing snapshots can be counted on the wire.
struct GroupFixture {
  Device device;
  LightService light;
  DistanceService distance;
  Controller controller{&distance, &device.service()};
  FakeMode first{"first", "First", &light, &distance, &device.service()};

  GroupFixture() {
    controller.addMode(&first);
    // Group scope publishes the state, so the lamp is not locally diverged and
    // is allowed to serve snapshots.
    JsonDocument select = request("mode.select", "first");
    select["scope"] = "group";
    CHECK(controller.executeControl(select)["ok"].as<bool>());
  }

  // Authenticates a peer with the given MAC so it shows up in getNodes().
  uint32_t join(const char* macHex) {
    Peer peer(macHex, "0f0e0d0c0b0a09080706050403020100", testGroupKey());
    CHECK(handshake(device, peer));
    return peer.nodeId();
  }

  size_t snapshotsSent() {
    size_t count = 0;
    for (const ParsedFrame& frame : device.outbox(Frame::DATA)) {
      std::string payload(frame.plaintext.begin(), frame.plaintext.end());
      if (payload.find("\"replyTo\"") != std::string::npos) ++count;
    }
    return count;
  }

  void deliverStateRequest(uint32_t requester) {
    JsonDocument message;
    message["kind"] = "state.request";
    message["requester"] = requester;
    GlowControllerTestAccess::receive(controller, requester, message, MessageType::SYNC);
    device.pump(8);
  }
};

}  // namespace

// Every peer answering at once floods a sixteen slot transmit queue with
// multi-fragment snapshots. Only the lowest node id replies, the same tie-break
// the connection callback already uses.
GLOW_TEST(only_the_lowest_node_answers_a_state_request) {
  GroupFixture fixture;
  uint32_t requester = fixture.join("aabbccddee01");
  // A third lamp with a node id below ours is the designated responder.
  fixture.join("000000000001");

  fixture.device.clearOutbox();
  fixture.deliverStateRequest(requester);

  CHECK_EQ(fixture.snapshotsSent(), static_cast<size_t>(0));
}

GLOW_TEST(the_lowest_node_does_answer_a_state_request) {
  GroupFixture fixture;
  uint32_t requester = fixture.join("ffffffffff01");
  fixture.join("ffffffffff02");

  fixture.device.clearOutbox();
  fixture.deliverStateRequest(requester);

  CHECK_EQ(fixture.snapshotsSent(), static_cast<size_t>(1));
}

// With no other candidate the only peer that can help must still answer.
GLOW_TEST(a_lone_peer_answers_a_state_request) {
  GroupFixture fixture;
  uint32_t requester = fixture.join("000000000001");

  fixture.device.clearOutbox();
  fixture.deliverStateRequest(requester);

  CHECK_EQ(fixture.snapshotsSent(), static_cast<size_t>(1));
}

GLOW_TEST(group_commands_reject_existing_local_divergence) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  glow_shim::clockMillis += 2001;
  GlowControllerTestAccess::maintain(fixture.controller);

  JsonDocument local = request("mode.setting.set", "first");
  local["setting"] = "speed";
  local["value"] = 4;
  fixture.controller.executeControl(local);
  uint64_t before = fixture.communication.syncVersion().revision;

  JsonDocument command = request("mode.command", "first");
  command["scope"] = "group";
  command["command"] = "toggle";
  command["arguments"].to<JsonObject>();
  JsonDocument response = fixture.controller.executeControl(command);
  CHECK(!response["ok"].as<bool>());
  CHECK(response["error"]["code"].as<String>() == "LOCAL_STATE_DIVERGED");

  JsonDocument state = fixture.controller.state();
  CHECK(state["sync"]["localDirty"].as<bool>());
  CHECK(!state["mode"]["registry"]["running"].as<bool>());
  CHECK_EQ(state["sync"]["revision"].as<uint64_t>(), before);
}

GLOW_TEST(a_command_event_cannot_complete_rejoining) {
  Fixture fixture;
  fixture.startTransport();
  fixture.controller.executeControl(request("mode.select", "first"));
  fixture.controller.executeControl(syncPolicy(false, false));
  fixture.controller.executeControl(syncPolicy(true, false));

  JsonDocument command = stateEvent(2, 5, 200);
  command["eventKind"] = static_cast<uint8_t>(ModeEventKind::COMMAND);
  command["command"] = "toggle";
  GlowControllerTestAccess::receive(fixture.controller, 200, command,
                                    MessageType::EVENT);

  JsonDocument state = fixture.controller.state();
  CHECK(state["sync"]["status"].as<String>() == "joining");
  CHECK(state["sync"]["localDirty"].as<bool>());
  CHECK(!state["mode"]["registry"]["running"].as<bool>());
}

GLOW_TEST(an_invalid_remote_command_does_not_switch_modes) {
  Fixture fixture;
  fixture.controller.executeControl(request("mode.select", "first"));
  JsonDocument command = stateEvent(2, 1, 200);
  command["eventKind"] = static_cast<uint8_t>(ModeEventKind::COMMAND);
  command["mode"]["id"] = "second";
  command["mode"]["title"] = "Second";
  command["command"] = "unknown";
  command["payload"].to<JsonObject>();

  GlowControllerTestAccess::receive(fixture.controller, 200, command,
                                    MessageType::EVENT);
  CHECK(fixture.controller.getCurrentModeId() == "first");
  CHECK_EQ(fixture.second.activations, 0);
}
