// Mapping between the control API and MQTT. The whole point of this layer is
// that it never mentions a concrete mode, so a new mode reaches Home Assistant
// without touching any of it.

#include <map>

#include "Controller.h"
#include "HomeAssistantProtocol.h"
#include "support.h"

using namespace glowtest;

namespace {

class ExtraMode : public AbstractMode {
 public:
  ExtraMode(LightService* light, DistanceService* distance,
            CommunicationService* communication, const char* modeId = "aurora",
            const char* modeTitle = "Aurora")
      : AbstractMode(light, distance, communication) {
    this->id = modeId;
    this->title = modeTitle;
    this->description = "A mode nobody told the MQTT layer about";
    this->version = "0.1.0";
  }

  void setup() override {
    this->registry.init("shimmer", RegistryType::INT, 3, 1, 9);
    this->registry.init("frozen", RegistryType::BOOL, false);
    this->addOption("Shimmer", []() {}, false);
    this->addOption("Freeze", []() {}, false);
  }

  void customFirst() override {}
  void customLoop() override {}
  void last() override {}
  void customClick() override {}
};

HomeAssistantIdentity identity() {
  HomeAssistantIdentity value;
  value.deviceId = "glow-42";
  value.deviceName = "GlowLight Test";
  value.baseTopic = "glowlight/glow-42";
  value.discoveryPrefix = "homeassistant";
  value.firmwareVersion = "test";
  return value;
}

struct Captured {
  std::map<std::string, std::string> byTopic;
  std::vector<std::string> order;

  HomeAssistantSink sink() {
    return [this](const String& topic, const String& payload, bool) {
      this->byTopic[topic.c_str()] = payload.c_str();
      this->order.push_back(topic.c_str());
      return true;
    };
  }

  bool has(const std::string& topic) const {
    return this->byTopic.find(topic) != this->byTopic.end();
  }

  std::string at(const std::string& topic) const {
    auto found = this->byTopic.find(topic);
    return found == this->byTopic.end() ? std::string() : found->second;
  }

  size_t countPrefixed(const std::string& prefix) const {
    size_t count = 0;
    for (const auto& entry : this->byTopic) {
      if (entry.first.rfind(prefix, 0) == 0) ++count;
    }
    return count;
  }
};

// A controller with two ordinary modes; tests add a third when they need to
// prove that nothing here knows about specific modes.
struct Lamp {
  LightService light;
  DistanceService distance;
  CommunicationService communication;
  Controller controller{&distance, &communication};
  ExtraMode extra{&light, &distance, &communication};

  JsonDocument capabilities() { return controller.capabilities(); }
  JsonDocument state() { return controller.state(); }
};

}  // namespace

GLOW_TEST(discovery_announces_one_device_with_a_light_and_a_mode_select) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  lamp.controller.executeControl([] {
    JsonDocument document;
    document["api"] = "glow.control/1";
    document["operation"] = "mode.select";
    document["target"]["mode"] = "aurora";
    return document;
  }());

  Captured captured;
  CHECK(HomeAssistantProtocol::buildDiscovery(identity(), lamp.capabilities(),
                                              captured.sink()));

  CHECK(captured.has("homeassistant/light/glow-42/light/config"));
  CHECK(captured.has("homeassistant/select/glow-42/mode/config"));

  JsonDocument light;
  CHECK(deserializeJson(light, captured.at("homeassistant/light/glow-42/light/config")) ==
        DeserializationError::Ok);
  CHECK(light["schema"].as<String>() == "json");
  CHECK(light["brightness"].as<bool>());
  CHECK(light["unique_id"].as<String>() == "glow-42_light");
  CHECK(light["device"]["identifiers"][0].as<String>() == "glow-42");
  CHECK(light["availability"][0]["topic"].as<String>() == "glowlight/glow-42/status");
}

// The guardrail: a mode that this file has never heard of has to show up with
// its options and its settings, purely from what the mode declared.
GLOW_TEST(an_unknown_mode_reaches_home_assistant_without_protocol_changes) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  Captured captured;
  CHECK(HomeAssistantProtocol::buildDiscovery(identity(), lamp.capabilities(),
                                              captured.sink()));

  // Its option select carries the registered option names.
  JsonDocument options;
  CHECK(captured.has("homeassistant/select/glow-42/option_aurora/config"));
  CHECK(deserializeJson(
            options, captured.at("homeassistant/select/glow-42/option_aurora/config")) ==
        DeserializationError::Ok);
  CHECK_EQ(options["options"].size(), static_cast<size_t>(2));
  CHECK(options["options"][0].as<String>() == "Shimmer");
  CHECK(options["options"][1].as<String>() == "Freeze");

  // An integer setting becomes a number with the declared bounds...
  JsonDocument shimmer;
  CHECK(captured.has("homeassistant/number/glow-42/setting_aurora_shimmer/config"));
  CHECK(deserializeJson(shimmer, captured.at(
                                     "homeassistant/number/glow-42/setting_aurora_shimmer/config")) ==
        DeserializationError::Ok);
  CHECK_EQ(shimmer["min"].as<int>(), 1);
  CHECK_EQ(shimmer["max"].as<int>(), 9);

  // ...and a boolean setting becomes a switch.
  CHECK(captured.has("homeassistant/switch/glow-42/setting_aurora_frozen/config"));
}

// Read-only values and brightness must not turn into a second control.
GLOW_TEST(read_only_settings_and_brightness_get_no_own_entity) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  Captured captured;
  CHECK(HomeAssistantProtocol::buildDiscovery(identity(), lamp.capabilities(),
                                              captured.sink()));

  CHECK(!captured.has("homeassistant/number/glow-42/setting_aurora_brightness/config"));
  CHECK(!captured.has(
      "homeassistant/number/glow-42/setting_aurora_currentOption/config"));
}

GLOW_TEST(entities_of_an_inactive_mode_are_marked_unavailable) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  Captured captured;
  CHECK(HomeAssistantProtocol::buildDiscovery(identity(), lamp.capabilities(),
                                              captured.sink()));

  JsonDocument shimmer;
  CHECK(deserializeJson(shimmer, captured.at(
                                     "homeassistant/number/glow-42/setting_aurora_shimmer/config")) ==
        DeserializationError::Ok);
  CHECK(shimmer["availability_mode"].as<String>() == "all");
  CHECK(shimmer["availability"][1]["topic"].as<String>() ==
        "glowlight/glow-42/mode/aurora/available");
}

GLOW_TEST(state_reports_brightness_as_light_on_or_off) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  JsonDocument select;
  select["api"] = "glow.control/1";
  select["operation"] = "mode.select";
  select["target"]["mode"] = "aurora";
  CHECK(lamp.controller.executeControl(select)["ok"].as<bool>());

  Captured captured;
  CHECK(HomeAssistantProtocol::buildState(identity(), lamp.capabilities(), lamp.state(),
                                          captured.sink()));

  JsonDocument light;
  CHECK(deserializeJson(light, captured.at("glowlight/glow-42/light/state")) ==
        DeserializationError::Ok);
  CHECK(light["state"].as<String>() == "ON");
  CHECK_EQ(light["brightness"].as<int>(), LED_DEFAULT_BRIGHTNESS);

  CHECK_EQ(captured.at("glowlight/glow-42/mode/state"), std::string("Aurora"));
  CHECK_EQ(captured.at("glowlight/glow-42/mode/aurora/available"), std::string("online"));
  CHECK_EQ(captured.at("glowlight/glow-42/setting/aurora/shimmer/state"),
           std::string("3"));
  CHECK_EQ(captured.at("glowlight/glow-42/setting/aurora/frozen/state"),
           std::string("false"));
}

GLOW_TEST(only_the_active_mode_publishes_setting_values) {
  Lamp lamp;
  ExtraMode other{&lamp.light, &lamp.distance, &lamp.communication, "borealis",
                  "Borealis"};
  lamp.controller.addMode(&lamp.extra);
  lamp.controller.addMode(&other);

  JsonDocument select;
  select["api"] = "glow.control/1";
  select["operation"] = "mode.select";
  select["target"]["mode"] = "aurora";
  CHECK(lamp.controller.executeControl(select)["ok"].as<bool>());

  Captured captured;
  CHECK(HomeAssistantProtocol::buildState(identity(), lamp.capabilities(), lamp.state(),
                                          captured.sink()));

  CHECK_EQ(captured.at("glowlight/glow-42/mode/borealis/available"),
           std::string("offline"));
  CHECK(!captured.has("glowlight/glow-42/setting/borealis/shimmer/state"));
}

// ------------------------------------------------------------- commands -----

namespace {

JsonDocument mapCommand(Lamp& lamp, const char* topic, const char* payload,
                        const char* scope = "group") {
  JsonDocument request;
  bool mapped = HomeAssistantProtocol::buildControlRequest(
      identity(), lamp.capabilities(), lamp.state(), topic, payload, scope, &request);
  if (!mapped) request.clear();
  return request;
}

void selectAurora(Lamp& lamp) {
  JsonDocument select;
  select["api"] = "glow.control/1";
  select["operation"] = "mode.select";
  select["target"]["mode"] = "aurora";
  CHECK(lamp.controller.executeControl(select)["ok"].as<bool>());
}

}  // namespace

GLOW_TEST(a_light_command_sets_the_brightness_of_the_active_mode) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  selectAurora(lamp);

  JsonDocument request = mapCommand(
      lamp, "glowlight/glow-42/light/set", "{\"state\":\"ON\",\"brightness\":42}");
  CHECK(request["operation"].as<String>() == "mode.setting.set");
  CHECK(request["target"]["mode"].as<String>() == "aurora");
  CHECK(request["setting"].as<String>() == "brightness");
  CHECK_EQ(request["value"].as<int>(), 42);
  CHECK(request["scope"].as<String>() == "group");

  // Switching off is brightness zero, and the lamp accepts that.
  JsonDocument off = mapCommand(lamp, "glowlight/glow-42/light/set",
                               "{\"state\":\"OFF\"}", "local");
  CHECK_EQ(off["value"].as<int>(), 0);
  CHECK(lamp.controller.executeControl(off)["ok"].as<bool>());
}

// Turning on without a brightness must not leave the lamp dark.
GLOW_TEST(switching_on_without_a_brightness_restores_a_visible_level) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  selectAurora(lamp);

  JsonDocument off = mapCommand(lamp, "glowlight/glow-42/light/set",
                               "{\"state\":\"OFF\"}", "local");
  CHECK(lamp.controller.executeControl(off)["ok"].as<bool>());

  JsonDocument on = mapCommand(lamp, "glowlight/glow-42/light/set",
                              "{\"state\":\"ON\"}", "local");
  CHECK(on["value"].as<int>() > 0);
}

GLOW_TEST(a_mode_command_maps_the_display_name_back_to_the_mode_id) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  JsonDocument request = mapCommand(lamp, "glowlight/glow-42/mode/set", "Aurora");
  CHECK(request["operation"].as<String>() == "mode.select");
  CHECK(request["target"]["mode"].as<String>() == "aurora");

  CHECK(mapCommand(lamp, "glowlight/glow-42/mode/set", "Nonexistent").isNull());
}

GLOW_TEST(setting_commands_are_typed_from_the_declared_schema) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  selectAurora(lamp);

  JsonDocument number =
      mapCommand(lamp, "glowlight/glow-42/setting/aurora/shimmer/set", "7", "local");
  CHECK(number["value"].is<int>());
  CHECK_EQ(number["value"].as<int>(), 7);
  CHECK(lamp.controller.executeControl(number)["ok"].as<bool>());

  JsonDocument boolean =
      mapCommand(lamp, "glowlight/glow-42/setting/aurora/frozen/set", "true", "local");
  CHECK(boolean["value"].is<bool>());
  CHECK(boolean["value"].as<bool>());
  CHECK(lamp.controller.executeControl(boolean)["ok"].as<bool>());
}

GLOW_TEST(an_option_command_maps_the_option_name_to_its_index) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  selectAurora(lamp);

  JsonDocument request =
      mapCommand(lamp, "glowlight/glow-42/option/aurora/set", "Freeze", "local");
  CHECK(request["operation"].as<String>() == "mode.option.set");
  CHECK_EQ(request["value"].isNull() ? request["option"].as<int>() : -1, 1);
  CHECK(lamp.controller.executeControl(request)["ok"].as<bool>());
}

// A sync switch must carry the other control along, or it would reset it.
GLOW_TEST(a_sync_switch_preserves_the_other_control) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  JsonDocument request =
      mapCommand(lamp, "glowlight/glow-42/sync/publish/set", "false");
  CHECK(request["operation"].as<String>() == "sync.configure");
  CHECK(request["scope"].as<String>() == "local");
  CHECK(!request["sync"]["publish"].as<bool>());
  CHECK(request["sync"]["follow"].as<bool>() ==
        lamp.state()["sync"]["follow"].as<bool>());
}

GLOW_TEST(unknown_and_foreign_topics_are_refused) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  CHECK(mapCommand(lamp, "glowlight/glow-42/nonsense/set", "x").isNull());
  CHECK(mapCommand(lamp, "glowlight/other-lamp/mode/set", "Aurora").isNull());
  CHECK(mapCommand(lamp, "glowlight/glow-42/setting/aurora/unknown/set", "1").isNull());
  CHECK(mapCommand(lamp, "glowlight/glow-42/light/set", "not json").isNull());
}

GLOW_TEST(subscriptions_stay_constant_regardless_of_the_number_of_modes) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);

  std::vector<std::string> topics;
  HomeAssistantProtocol::buildSubscriptions(
      identity(), lamp.capabilities(),
      [&topics](const String& topic) { topics.push_back(topic.c_str()); });

  size_t withOneMode = topics.size();
  ExtraMode other{&lamp.light, &lamp.distance, &lamp.communication, "borealis",
                  "Borealis"};
  lamp.controller.addMode(&other);

  topics.clear();
  HomeAssistantProtocol::buildSubscriptions(
      identity(), lamp.capabilities(),
      [&topics](const String& topic) { topics.push_back(topic.c_str()); });

  CHECK_EQ(topics.size(), withOneMode);
}

// The group key must never appear in anything this layer publishes.
GLOW_TEST(no_published_payload_contains_the_group_key) {
  Lamp lamp;
  lamp.controller.addMode(&lamp.extra);
  selectAurora(lamp);

  Captured captured;
  CHECK(HomeAssistantProtocol::buildDiscovery(identity(), lamp.capabilities(),
                                              captured.sink()));
  CHECK(HomeAssistantProtocol::buildState(identity(), lamp.capabilities(), lamp.state(),
                                          captured.sink()));

  std::string key = GLOW_GROUP_KEY_HEX;
  for (const auto& entry : captured.byTopic) {
    CHECK(entry.second.find(key) == std::string::npos);
    CHECK(entry.second.find("groupKey") == std::string::npos);
  }
}
