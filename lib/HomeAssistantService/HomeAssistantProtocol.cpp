#include "HomeAssistantProtocol.h"

namespace {

// The light entity already carries brightness, and read-only values are not
// controllable, so neither gets a second entity of its own.
const char BRIGHTNESS_KEY[] = "brightness";

String joinTopic(const String& base, const String& suffix) {
  return base + "/" + suffix;
}

String settingTopic(const HomeAssistantIdentity& identity, const String& modeId,
                    const String& key, const char* leaf) {
  return identity.baseTopic + "/setting/" + modeId + "/" + key + "/" + leaf;
}

String uniqueId(const HomeAssistantIdentity& identity, const String& suffix) {
  return identity.deviceId + "_" + suffix;
}

String discoveryTopic(const HomeAssistantIdentity& identity, const char* component,
                      const String& objectId) {
  return identity.discoveryPrefix + "/" + component + "/" + identity.deviceId + "/" +
         objectId + "/config";
}

// Home Assistant groups entities that share this block into one device.
void addDeviceBlock(JsonDocument& document, const HomeAssistantIdentity& identity) {
  document["device"]["identifiers"][0] = identity.deviceId;
  document["device"]["name"] = identity.deviceName;
  document["device"]["manufacturer"] = "GlowLight";
  document["device"]["model"] = "GlowLight ESP32-C3";
  if (!identity.firmwareVersion.isEmpty()) {
    document["device"]["sw_version"] = identity.firmwareVersion;
  }
}

void addDeviceAvailability(JsonDocument& document,
                           const HomeAssistantIdentity& identity) {
  document["availability"][0]["topic"] =
      HomeAssistantProtocol::availabilityTopic(identity);
}

// Entities of an inactive mode stay visible but unavailable, which keeps the
// retained discovery valid across mode changes.
void addModeAvailability(JsonDocument& document,
                         const HomeAssistantIdentity& identity, const String& modeId) {
  addDeviceAvailability(document, identity);
  document["availability"][1]["topic"] =
      HomeAssistantProtocol::modeAvailabilityTopic(identity, modeId);
  document["availability_mode"] = "all";
}

bool emitDocument(const HomeAssistantSink& emit, const String& topic,
                  const JsonDocument& document, bool retain) {
  String payload;
  serializeJson(document, payload);
  return emit(topic, payload, retain);
}

bool isControllableSetting(JsonObjectConst setting) {
  return setting["writable"].as<bool>();
}

JsonObjectConst activeModeCapabilities(const JsonDocument& capabilities,
                                       const String& modeId) {
  for (JsonObjectConst mode : capabilities["modes"].as<JsonArrayConst>()) {
    if (mode["id"].as<String>() == modeId) return mode;
  }
  return JsonObjectConst();
}

}  // namespace

namespace HomeAssistantProtocol {

String availabilityTopic(const HomeAssistantIdentity& identity) {
  return joinTopic(identity.baseTopic, "status");
}

String stateTopic(const HomeAssistantIdentity& identity) {
  return joinTopic(identity.baseTopic, "state");
}

String modeAvailabilityTopic(const HomeAssistantIdentity& identity,
                             const String& modeId) {
  return identity.baseTopic + "/mode/" + modeId + "/available";
}

bool buildDiscovery(const HomeAssistantIdentity& identity,
                    const JsonDocument& capabilities, const HomeAssistantSink& emit) {
  // Brightness is the one control every mode inherits from AbstractMode, so it
  // is the only thing the light entity can rely on being present.
  {
    JsonDocument light;
    light["name"] = "Light";
    light["unique_id"] = uniqueId(identity, "light");
    light["schema"] = "json";
    light["brightness"] = true;
    light["brightness_scale"] = 255;
    light["state_topic"] = identity.baseTopic + "/light/state";
    light["command_topic"] = identity.baseTopic + "/light/set";
    addDeviceAvailability(light, identity);
    addDeviceBlock(light, identity);
    if (!emitDocument(emit, discoveryTopic(identity, "light", "light"), light, true)) {
      return false;
    }
  }

  {
    JsonDocument select;
    select["name"] = "Mode";
    select["unique_id"] = uniqueId(identity, "mode");
    select["state_topic"] = identity.baseTopic + "/mode/state";
    select["command_topic"] = identity.baseTopic + "/mode/set";
    JsonArray options = select["options"].to<JsonArray>();
    for (JsonObjectConst mode : capabilities["modes"].as<JsonArrayConst>()) {
      options.add(mode["name"].as<String>());
    }
    addDeviceAvailability(select, identity);
    addDeviceBlock(select, identity);
    if (!emitDocument(emit, discoveryTopic(identity, "select", "mode"), select, true)) {
      return false;
    }
  }

  for (JsonObjectConst mode : capabilities["modes"].as<JsonArrayConst>()) {
    String modeId = mode["id"].as<String>();
    String modeName = mode["name"].as<String>();

    // Options differ per mode, and Home Assistant fixes a select's options at
    // discovery time, so every mode gets its own entity.
    JsonArrayConst options = mode["options"].as<JsonArrayConst>();
    if (!options.isNull() && options.size() > 0) {
      JsonDocument select;
      select["name"] = modeName + " option";
      select["unique_id"] = uniqueId(identity, "option_" + modeId);
      select["state_topic"] = identity.baseTopic + "/option/" + modeId + "/state";
      select["command_topic"] = identity.baseTopic + "/option/" + modeId + "/set";
      JsonArray names = select["options"].to<JsonArray>();
      for (JsonObjectConst option : options) {
        if (option["disabled"].as<bool>()) continue;
        names.add(option["name"].as<String>());
      }
      addModeAvailability(select, identity, modeId);
      addDeviceBlock(select, identity);
      if (!emitDocument(emit, discoveryTopic(identity, "select", "option_" + modeId),
                        select, true)) {
        return false;
      }
    }

    JsonObjectConst settings = mode["settings"].as<JsonObjectConst>();
    for (JsonPairConst entry : settings) {
      String key = entry.key().c_str();
      JsonObjectConst setting = entry.value().as<JsonObjectConst>();
      if (!isControllableSetting(setting) || key == BRIGHTNESS_KEY) continue;

      String type = setting["type"].as<String>();
      String objectId = "setting_" + modeId + "_" + key;
      JsonDocument entity;
      entity["name"] = modeName + " " + key;
      entity["unique_id"] = uniqueId(identity, objectId);
      entity["state_topic"] = settingTopic(identity, modeId, key, "state");
      entity["command_topic"] = settingTopic(identity, modeId, key, "set");
      addModeAvailability(entity, identity, modeId);
      addDeviceBlock(entity, identity);

      const char* component = nullptr;
      if (type == "integer") {
        component = "number";
        entity["min"] = setting["minimum"];
        entity["max"] = setting["maximum"];
        entity["mode"] = "slider";
      } else if (type == "boolean") {
        component = "switch";
        entity["payload_on"] = "true";
        entity["payload_off"] = "false";
      } else if (type == "string") {
        component = "text";
      } else {
        continue;  // unknown type, nothing sensible to offer
      }

      if (!emitDocument(emit, discoveryTopic(identity, component, objectId), entity,
                        true)) {
        return false;
      }
    }
  }

  if (capabilities["features"]["syncPolicy"]["supported"].as<bool>()) {
    struct SyncSwitch {
      const char* key;
      const char* name;
    };
    const SyncSwitch switches[] = {{"follow", "Follow group"},
                                   {"publish", "Publish to group"}};
    for (const SyncSwitch& entry : switches) {
      JsonDocument document;
      document["name"] = entry.name;
      document["unique_id"] = uniqueId(identity, String("sync_") + entry.key);
      document["state_topic"] = identity.baseTopic + "/sync/" + entry.key + "/state";
      document["command_topic"] = identity.baseTopic + "/sync/" + entry.key + "/set";
      document["payload_on"] = "true";
      document["payload_off"] = "false";
      document["entity_category"] = "config";
      addDeviceAvailability(document, identity);
      addDeviceBlock(document, identity);
      if (!emitDocument(emit,
                        discoveryTopic(identity, "switch", String("sync_") + entry.key),
                        document, true)) {
        return false;
      }
    }

    JsonDocument status;
    status["name"] = "Sync status";
    status["unique_id"] = uniqueId(identity, "sync_status");
    status["state_topic"] = identity.baseTopic + "/sync/status";
    status["entity_category"] = "diagnostic";
    status["json_attributes_topic"] = stateTopic(identity);
    addDeviceAvailability(status, identity);
    addDeviceBlock(status, identity);
    if (!emitDocument(emit, discoveryTopic(identity, "sensor", "sync_status"), status,
                      true)) {
      return false;
    }
  }

  return true;
}

bool buildState(const HomeAssistantIdentity& identity,
                const JsonDocument& capabilities, const JsonDocument& state,
                const HomeAssistantSink& emit) {
  String activeModeId = state["mode"]["id"].as<String>();
  JsonObjectConst registry = state["mode"]["registry"].as<JsonObjectConst>();

  // Full document first, so template users always see a consistent snapshot.
  if (!emitDocument(emit, stateTopic(identity), state, true)) return false;

  {
    JsonDocument light;
    uint16_t brightness = registry[BRIGHTNESS_KEY] | 0;
    light["state"] = brightness > 0 ? "ON" : "OFF";
    light["brightness"] = brightness;
    if (!emitDocument(emit, identity.baseTopic + "/light/state", light, true)) {
      return false;
    }
  }

  JsonObjectConst activeMode = activeModeCapabilities(capabilities, activeModeId);
  if (!activeMode.isNull()) {
    if (!emit(identity.baseTopic + "/mode/state", activeMode["name"].as<String>(),
              true)) {
      return false;
    }
  }

  for (JsonObjectConst mode : capabilities["modes"].as<JsonArrayConst>()) {
    String modeId = mode["id"].as<String>();
    bool active = modeId == activeModeId;
    if (!emit(modeAvailabilityTopic(identity, modeId), active ? "online" : "offline",
              true)) {
      return false;
    }
    if (!active) continue;

    JsonArrayConst options = mode["options"].as<JsonArrayConst>();
    if (!options.isNull() && options.size() > 0) {
      uint8_t current = registry["currentOption"] | 0;
      for (JsonObjectConst option : options) {
        if (option["index"].as<uint8_t>() != current) continue;
        if (!emit(identity.baseTopic + "/option/" + modeId + "/state",
                  option["name"].as<String>(), true)) {
          return false;
        }
        break;
      }
    }

    for (JsonPairConst entry : mode["settings"].as<JsonObjectConst>()) {
      String key = entry.key().c_str();
      JsonObjectConst setting = entry.value().as<JsonObjectConst>();
      if (!isControllableSetting(setting) || key == BRIGHTNESS_KEY) continue;
      if (registry[key].isNull()) continue;

      String value;
      if (setting["type"].as<String>() == "boolean") {
        value = registry[key].as<bool>() ? "true" : "false";
      } else {
        value = registry[key].as<String>();
      }
      if (!emit(settingTopic(identity, modeId, key, "state"), value, true)) return false;
    }
  }

  if (capabilities["features"]["syncPolicy"]["supported"].as<bool>()) {
    if (!emit(identity.baseTopic + "/sync/follow/state",
              state["sync"]["follow"].as<bool>() ? "true" : "false", true)) {
      return false;
    }
    if (!emit(identity.baseTopic + "/sync/publish/state",
              state["sync"]["publish"].as<bool>() ? "true" : "false", true)) {
      return false;
    }
    if (!emit(identity.baseTopic + "/sync/status", state["sync"]["status"].as<String>(),
              true)) {
      return false;
    }
  }

  return true;
}

void buildSubscriptions(const HomeAssistantIdentity& identity,
                        const JsonDocument& capabilities,
                        const std::function<void(const String&)>& subscribe) {
  subscribe(identity.baseTopic + "/light/set");
  subscribe(identity.baseTopic + "/mode/set");
  // One wildcard each keeps the subscription count independent of the number of
  // modes and settings, which a broker limits far sooner than the topic depth.
  subscribe(identity.baseTopic + "/option/+/set");
  subscribe(identity.baseTopic + "/setting/+/+/set");
  if (capabilities["features"]["syncPolicy"]["supported"].as<bool>()) {
    subscribe(identity.baseTopic + "/sync/+/set");
  }
}

bool buildControlRequest(const HomeAssistantIdentity& identity,
                         const JsonDocument& capabilities,
                         const JsonDocument& state, const String& topic,
                         const String& payload, const String& scope,
                         JsonDocument* request) {
  if (!topic.startsWith(identity.baseTopic + "/")) return false;
  String path = topic.substring(identity.baseTopic.length() + 1);
  String activeModeId = state["mode"]["id"].as<String>();

  request->clear();
  (*request)["api"] = "glow.control/1";
  (*request)["requestId"] = "mqtt";

  if (path == "light/set") {
    JsonDocument command;
    if (deserializeJson(command, payload) != DeserializationError::Ok) return false;
    if (activeModeId.isEmpty()) return false;

    uint16_t brightness = 0;
    if (!command["brightness"].isNull()) {
      brightness = command["brightness"].as<uint16_t>();
    } else if (command["state"].as<String>() == "ON") {
      // Turning on without a brightness keeps whatever the lamp had, so fall
      // back to the current value and only ever raise it off zero.
      uint16_t current =
          state["mode"]["registry"][BRIGHTNESS_KEY] | static_cast<uint16_t>(0);
      brightness = current > 0 ? current : 255;
    }
    if (command["state"].as<String>() == "OFF") brightness = 0;
    if (brightness > 255) brightness = 255;

    (*request)["operation"] = "mode.setting.set";
    (*request)["scope"] = scope;
    (*request)["target"]["mode"] = activeModeId;
    (*request)["setting"] = BRIGHTNESS_KEY;
    (*request)["value"] = brightness;
    return true;
  }

  if (path == "mode/set") {
    for (JsonObjectConst mode : capabilities["modes"].as<JsonArrayConst>()) {
      if (mode["name"].as<String>() != payload) continue;
      (*request)["operation"] = "mode.select";
      (*request)["scope"] = scope;
      (*request)["target"]["mode"] = mode["id"];
      return true;
    }
    return false;
  }

  if (path.startsWith("option/") && path.endsWith("/set")) {
    String modeId = path.substring(7, path.length() - 4);
    JsonObjectConst mode = activeModeCapabilities(capabilities, modeId);
    if (mode.isNull()) return false;
    for (JsonObjectConst option : mode["options"].as<JsonArrayConst>()) {
      if (option["name"].as<String>() != payload) continue;
      (*request)["operation"] = "mode.option.set";
      (*request)["scope"] = scope;
      (*request)["target"]["mode"] = modeId;
      (*request)["option"] = option["index"];
      return true;
    }
    return false;
  }

  if (path.startsWith("setting/") && path.endsWith("/set")) {
    String body = path.substring(8, path.length() - 4);
    int separator = body.indexOf('/');
    if (separator <= 0) return false;
    String modeId = body.substring(0, separator);
    String key = body.substring(separator + 1);

    JsonObjectConst mode = activeModeCapabilities(capabilities, modeId);
    if (mode.isNull()) return false;
    JsonObjectConst setting = mode["settings"][key].as<JsonObjectConst>();
    if (setting.isNull() || !isControllableSetting(setting)) return false;

    (*request)["operation"] = "mode.setting.set";
    (*request)["scope"] = scope;
    (*request)["target"]["mode"] = modeId;
    (*request)["setting"] = key;

    String type = setting["type"].as<String>();
    if (type == "integer") {
      (*request)["value"] = payload.toInt();
    } else if (type == "boolean") {
      (*request)["value"] = payload == "true" || payload == "ON" || payload == "1";
    } else {
      (*request)["value"] = payload;
    }
    return true;
  }

  if (path.startsWith("sync/") && path.endsWith("/set")) {
    String control = path.substring(5, path.length() - 4);
    if (control != "follow" && control != "publish") return false;
    bool enabled = payload == "true" || payload == "ON" || payload == "1";

    // Both controls have to be sent together, so the untouched one keeps its
    // current value.
    (*request)["operation"] = "sync.configure";
    (*request)["scope"] = "local";
    (*request)["sync"]["follow"] = control == "follow"
                                       ? enabled
                                       : state["sync"]["follow"].as<bool>();
    (*request)["sync"]["publish"] = control == "publish"
                                        ? enabled
                                        : state["sync"]["publish"].as<bool>();
    return true;
  }

  return false;
}

}  // namespace HomeAssistantProtocol
