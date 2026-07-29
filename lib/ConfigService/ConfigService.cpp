#include "ConfigService.h"

namespace {
void setError(String* error, const char* message) {
  if (error != nullptr) *error = message;
}

bool isHex(char character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}
}  // namespace

DeviceConfig DeviceConfig::compileTimeDefaults() { return DeviceConfig(); }

DeviceConfig DeviceConfig::safeDefaults() {
  DeviceConfig config;
  config.wifiEnabled = false;
  config.wifiSsid = "";
  config.wifiPassword = "";
  config.hostname = "glowlight";
  config.fallbackChannel = 1;
  config.communicationEnabled = false;
  config.groupKeyHex = "";
  config.syncFollow = true;
  config.syncPublish = true;
  config.otaEnabled = false;
  config.otaPassword = "";
  config.mqttEnabled = false;
  config.mqttHost = "";
  config.mqttUser = "";
  config.mqttPassword = "";
  return config;
}

String DeviceConfig::uniqueHostname(const String& configured, const uint8_t* mac,
                                    const char* defaultHostname) {
  String base = configured.isEmpty() ? String(defaultHostname) : configured;
  if (base != defaultHostname || mac == nullptr) return base;

  char suffix[8];
  snprintf(suffix, sizeof(suffix), "-%02x%02x%02x", mac[3], mac[4], mac[5]);

  // A hostname may not exceed 63 characters, and the suffix is what makes it
  // unique, so the base is what gives way.
  const size_t suffixLength = strlen(suffix);
  if (base.length() + suffixLength > 63) {
    base = base.substring(0, 63 - suffixLength);
  }
  return base + suffix;
}

bool DeviceConfig::validHostname(const String& value) {
  if (value.isEmpty() || value.length() > 63 || value[0] == '-' ||
      value[value.length() - 1] == '-') return false;
  for (size_t i = 0; i < value.length(); ++i) {
    char character = value[i];
    if (!((character >= 'a' && character <= 'z') ||
          (character >= 'A' && character <= 'Z') ||
          (character >= '0' && character <= '9') || character == '-')) return false;
  }
  return true;
}

bool DeviceConfig::validGroupKey(const String& value) {
  if (value.length() != 64) return false;
  bool nonZero = false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isHex(value[i])) return false;
    if (value[i] != '0') nonZero = true;
  }
  return nonZero;
}

bool DeviceConfig::validWifiPassword(const String& value) {
  if (value.isEmpty()) return true;
  if (value.length() >= 8 && value.length() <= 63) return true;
  if (value.length() != 64) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isHex(value[i])) return false;
  }
  return true;
}

bool DeviceConfig::validMqttHost(const String& value) {
  if (value.isEmpty() || value.length() > 253) return false;
  for (unsigned int i = 0; i < value.length(); ++i) {
    char c = value[i];
    bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':';
    if (!allowed) return false;
  }
  return true;
}

bool DeviceConfig::validOtaPassword(const String& value) {
  return value.length() >= 12 && value.length() <= 63 &&
         value != "PROVISION_WITH_SETUP";
}

bool DeviceConfig::validate(String* error) const {
  if (this->fallbackChannel < 1 || this->fallbackChannel > 13) {
    setError(error, "Fallback channel must be between 1 and 13");
    return false;
  }
  if (!validHostname(this->hostname)) {
    setError(error, "Hostname must be a valid DNS label with at most 63 characters");
    return false;
  }
  if (this->wifiSsid.length() > 32 ||
      (this->wifiEnabled && this->wifiSsid.isEmpty())) {
    setError(error, "Enabled WiFi requires an SSID of at most 32 characters");
    return false;
  }
  if (!validWifiPassword(this->wifiPassword)) {
    setError(error, "WiFi password must be empty, 8-63 characters, or a 64 digit PSK");
    return false;
  }
  if (this->communicationEnabled && !validGroupKey(this->groupKeyHex)) {
    setError(error, "Enabled group communication requires a valid 256-bit key");
    return false;
  }
  if (this->otaEnabled && !validOtaPassword(this->otaPassword)) {
    setError(error, "Enabled OTA requires a unique password with 12-63 characters");
    return false;
  }
  if (this->mqttEnabled) {
    if (!this->wifiEnabled) {
      setError(error, "Home Assistant needs infrastructure WiFi");
      return false;
    }
    if (!validMqttHost(this->mqttHost)) {
      setError(error, "Enabled Home Assistant requires a valid broker host");
      return false;
    }
    if (this->mqttPort == 0) {
      setError(error, "Enabled Home Assistant requires a broker port");
      return false;
    }
    if (this->mqttDiscoveryPrefix.isEmpty() ||
        this->mqttDiscoveryPrefix.length() > 48) {
      setError(error, "Discovery prefix must be 1-48 characters");
      return false;
    }
  }
  return true;
}

String DeviceConfig::serialize() const {
  JsonDocument document;
  document["schema"] = "glow.config";
  document["schemaVersion"] = SCHEMA_VERSION;
  document["wifi"]["enabled"] = this->wifiEnabled;
  document["wifi"]["ssid"] = this->wifiSsid;
  document["wifi"]["password"] = this->wifiPassword;
  document["wifi"]["hostname"] = this->hostname;
  document["radio"]["fallbackChannel"] = this->fallbackChannel;
  document["group"]["enabled"] = this->communicationEnabled;
  document["group"]["key"] = this->groupKeyHex;
  document["group"]["follow"] = this->syncFollow;
  document["group"]["publish"] = this->syncPublish;
  document["ota"]["enabled"] = this->otaEnabled;
  document["ota"]["password"] = this->otaPassword;
  document["mqtt"]["enabled"] = this->mqttEnabled;
  document["mqtt"]["host"] = this->mqttHost;
  document["mqtt"]["port"] = this->mqttPort;
  document["mqtt"]["user"] = this->mqttUser;
  document["mqtt"]["password"] = this->mqttPassword;
  document["mqtt"]["discoveryPrefix"] = this->mqttDiscoveryPrefix;
  String serialized;
  serializeJson(document, serialized);
  return serialized;
}

JsonDocument DeviceConfig::redacted() const {
  JsonDocument document;
  document["api"] = "glow.config/1";
  document["wifi"]["enabled"] = this->wifiEnabled;
  document["wifi"]["ssid"] = this->wifiSsid;
  document["wifi"]["passwordConfigured"] = !this->wifiPassword.isEmpty();
  document["wifi"]["hostname"] = this->hostname;
  document["radio"]["fallbackChannel"] = this->fallbackChannel;
  document["group"]["enabled"] = this->communicationEnabled;
  document["group"]["keyConfigured"] = validGroupKey(this->groupKeyHex);
  document["group"]["follow"] = this->syncFollow;
  document["group"]["publish"] = this->syncPublish;
  document["ota"]["enabled"] = this->otaEnabled;
  document["ota"]["passwordConfigured"] = validOtaPassword(this->otaPassword);
  document["mqtt"]["enabled"] = this->mqttEnabled;
  document["mqtt"]["host"] = this->mqttHost;
  document["mqtt"]["port"] = this->mqttPort;
  document["mqtt"]["user"] = this->mqttUser;
  document["mqtt"]["passwordConfigured"] = !this->mqttPassword.isEmpty();
  document["mqtt"]["discoveryPrefix"] = this->mqttDiscoveryPrefix;
  return document;
}

bool DeviceConfig::fromJson(JsonObjectConst source, const DeviceConfig& current,
                            DeviceConfig* output, String* error) {
  if (output == nullptr || !source["wifi"].is<JsonObjectConst>() ||
      !source["radio"].is<JsonObjectConst>() ||
      !source["group"].is<JsonObjectConst>() ||
      !source["ota"].is<JsonObjectConst>()) {
    setError(error, "wifi, radio, group, and ota objects are required");
    return false;
  }
  JsonObjectConst wifi = source["wifi"].as<JsonObjectConst>();
  JsonObjectConst radio = source["radio"].as<JsonObjectConst>();
  JsonObjectConst group = source["group"].as<JsonObjectConst>();
  JsonObjectConst ota = source["ota"].as<JsonObjectConst>();
  if (!wifi["enabled"].is<bool>() || !wifi["ssid"].is<String>() ||
      !wifi["hostname"].is<String>() ||
      !radio["fallbackChannel"].is<uint8_t>() ||
      !group["enabled"].is<bool>() || !group["follow"].is<bool>() ||
      !group["publish"].is<bool>() || !ota["enabled"].is<bool>()) {
    setError(error, "Configuration fields have invalid or missing types");
    return false;
  }

  DeviceConfig candidate = current;
  candidate.wifiEnabled = wifi["enabled"].as<bool>();
  candidate.wifiSsid = wifi["ssid"].as<String>();
  candidate.hostname = wifi["hostname"].as<String>();
  candidate.fallbackChannel = radio["fallbackChannel"].as<uint8_t>();
  candidate.communicationEnabled = group["enabled"].as<bool>();
  candidate.syncFollow = group["follow"].as<bool>();
  candidate.syncPublish = group["publish"].as<bool>();
  candidate.otaEnabled = ota["enabled"].as<bool>();
  // The mqtt block is optional so older clients and the schema 1/2 migration
  // keep working; anything present is validated like every other field.
  if (source["mqtt"].is<JsonObjectConst>()) {
    JsonObjectConst mqtt = source["mqtt"].as<JsonObjectConst>();
    if (!mqtt["enabled"].is<bool>()) {
      setError(error, "Configuration fields have invalid or missing types");
      return false;
    }
    candidate.mqttEnabled = mqtt["enabled"].as<bool>();
    if (mqtt["host"].is<String>()) candidate.mqttHost = mqtt["host"].as<String>();
    if (mqtt["port"].is<uint16_t>()) candidate.mqttPort = mqtt["port"].as<uint16_t>();
    if (mqtt["user"].is<String>()) candidate.mqttUser = mqtt["user"].as<String>();
    if (mqtt["discoveryPrefix"].is<String>() &&
        !mqtt["discoveryPrefix"].as<String>().isEmpty()) {
      candidate.mqttDiscoveryPrefix = mqtt["discoveryPrefix"].as<String>();
    }
    if (mqtt["password"].is<String>() && !mqtt["password"].as<String>().isEmpty()) {
      candidate.mqttPassword = mqtt["password"].as<String>();
    }
  }
  if (wifi["password"].is<String>() && !wifi["password"].as<String>().isEmpty()) {
    candidate.wifiPassword = wifi["password"].as<String>();
  }
  if (group["key"].is<String>() && !group["key"].as<String>().isEmpty()) {
    candidate.groupKeyHex = group["key"].as<String>();
  }
  if (ota["password"].is<String>() && !ota["password"].as<String>().isEmpty()) {
    candidate.otaPassword = ota["password"].as<String>();
  }
  if (!candidate.validate(error)) return false;
  *output = candidate;
  return true;
}

bool DeviceConfig::deserialize(const String& serialized, DeviceConfig* output,
                               String* error) {
  JsonDocument document;
  DeserializationError jsonError = deserializeJson(document, serialized);
  if (jsonError) {
    setError(error, "Stored configuration is not valid JSON");
    return false;
  }
  if (document["schema"].as<String>() != "glow.config" ||
      !document["schemaVersion"].is<uint16_t>()) {
    setError(error, "Stored configuration schema is not supported");
    return false;
  }
  uint16_t schemaVersion = document["schemaVersion"].as<uint16_t>();
  if (schemaVersion == 1) {
    document["ota"]["enabled"] = false;
    document["ota"]["password"] = "";
    document["mqtt"]["enabled"] = false;
  } else if (schemaVersion == 2) {
    document["mqtt"]["enabled"] = false;
  } else if (schemaVersion != SCHEMA_VERSION) {
    setError(error, "Stored configuration schema is not supported");
    return false;
  }
  DeviceConfig empty;
  empty.wifiPassword = "";
  empty.groupKeyHex = "";
  empty.otaPassword = "";
  empty.mqttPassword = "";
  return fromJson(document.as<JsonObjectConst>(), empty, output, error);
}
