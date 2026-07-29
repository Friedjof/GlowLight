#include "ConfigService.h"
#include "PreferencesConfigStore.h"
#include "Preferences.h"
#include "support.h"

using namespace glowtest;

namespace {
DeviceConfig validConfig() {
  DeviceConfig config;
  config.wifiEnabled = true;
  config.wifiSsid = "home";
  config.wifiPassword = "password123";
  config.hostname = "glow-bedroom";
  config.fallbackChannel = 6;
  config.communicationEnabled = true;
  config.groupKeyHex =
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
  config.syncFollow = true;
  config.syncPublish = false;
  config.otaEnabled = true;
  config.otaPassword = "unique-ota-password";
  return config;
}
}  // namespace

GLOW_TEST(compile_time_device_configuration_is_valid) {
  DeviceConfig config = DeviceConfig::compileTimeDefaults();
  String error;
  CHECK(config.validate(&error));
}

GLOW_TEST(device_configuration_round_trips_without_losing_secrets) {
  DeviceConfig expected = validConfig();
  DeviceConfig actual;
  String error;
  CHECK(DeviceConfig::deserialize(expected.serialize(), &actual, &error));
  CHECK(actual.wifiEnabled);
  CHECK(actual.wifiSsid == "home");
  CHECK(actual.wifiPassword == "password123");
  CHECK(actual.hostname == "glow-bedroom");
  CHECK_EQ(actual.fallbackChannel, static_cast<uint8_t>(6));
  CHECK(actual.communicationEnabled);
  CHECK(actual.groupKeyHex == expected.groupKeyHex);
  CHECK(actual.syncFollow);
  CHECK(!actual.syncPublish);
  CHECK(actual.otaEnabled);
  CHECK(actual.otaPassword == "unique-ota-password");
}

GLOW_TEST(device_configuration_rejects_invalid_security_and_radio_values) {
  DeviceConfig config = validConfig();
  config.fallbackChannel = 14;
  CHECK(!config.validate());

  config = validConfig();
  config.groupKeyHex = String(std::string(64, '0'));
  CHECK(!config.validate());

  config = validConfig();
  config.hostname = "-invalid";
  CHECK(!config.validate());

  config = validConfig();
  config.wifiPassword = "short";
  CHECK(!config.validate());
}

GLOW_TEST(redacted_configuration_never_contains_passwords_or_group_keys) {
  DeviceConfig config = validConfig();
  JsonDocument redacted = config.redacted();
  String serialized;
  serializeJson(redacted, serialized);

  CHECK(redacted["wifi"]["passwordConfigured"].as<bool>());
  CHECK(redacted["group"]["keyConfigured"].as<bool>());
  CHECK(serialized.str().find("password123") == std::string::npos);
  CHECK(serialized.str().find(config.groupKeyHex.c_str()) == std::string::npos);
  CHECK(redacted["ota"]["passwordConfigured"].as<bool>());
  CHECK(serialized.str().find(config.otaPassword.c_str()) == std::string::npos);
}

GLOW_TEST(portal_updates_preserve_write_only_secrets_when_left_empty) {
  DeviceConfig current = validConfig();
  JsonDocument request;
  request["wifi"]["enabled"] = true;
  request["wifi"]["ssid"] = "new-home";
  request["wifi"]["password"] = "";
  request["wifi"]["hostname"] = "glow-new";
  request["radio"]["fallbackChannel"] = 11;
  request["group"]["enabled"] = true;
  request["group"]["key"] = "";
  request["group"]["follow"] = false;
  request["group"]["publish"] = true;
  request["ota"]["enabled"] = true;
  request["ota"]["password"] = "";

  DeviceConfig updated;
  String error;
  CHECK(DeviceConfig::fromJson(request.as<JsonObjectConst>(), current, &updated,
                               &error));
  CHECK(updated.wifiSsid == "new-home");
  CHECK(updated.wifiPassword == current.wifiPassword);
  CHECK(updated.groupKeyHex == current.groupKeyHex);
  CHECK_EQ(updated.fallbackChannel, static_cast<uint8_t>(11));
  CHECK(updated.otaPassword == current.otaPassword);
}

GLOW_TEST(schema_one_configuration_migrates_with_ota_disabled) {
  DeviceConfig config = validConfig();
  JsonDocument legacy;
  CHECK(!deserializeJson(legacy, config.serialize()));
  legacy["schemaVersion"] = 1;
  legacy.remove("ota");
  String serialized;
  serializeJson(legacy, serialized);

  DeviceConfig migrated;
  String error;
  CHECK(DeviceConfig::deserialize(serialized, &migrated, &error));
  CHECK(!migrated.otaEnabled);
  CHECK(migrated.otaPassword.isEmpty());
  CHECK(migrated.groupKeyHex == config.groupKeyHex);
}

// Falling back to safe defaults has to silence every subsystem, otherwise a
// rejected configuration still starts services with values nobody validated.
// All lamps share one compiled configuration, so the default hostname would be
// identical everywhere: they would all claim glowlight.local and show up in
// Home Assistant under the same name.
GLOW_TEST(the_default_hostname_gets_a_per_device_suffix) {
  uint8_t mac[6] = {0x98, 0x3d, 0xae, 0x52, 0xc8, 0x2c};
  CHECK(DeviceConfig::uniqueHostname(GLOW_HOSTNAME, mac) == "glowlight-52c82c");

  uint8_t other[6] = {0xdc, 0x06, 0x75, 0x9d, 0x6f, 0xc0};
  CHECK(DeviceConfig::uniqueHostname(GLOW_HOSTNAME, other) == "glowlight-9d6fc0");
  CHECK(DeviceConfig::uniqueHostname(GLOW_HOSTNAME, mac) !=
        DeviceConfig::uniqueHostname(GLOW_HOSTNAME, other));
}

// A name entered in the portal belongs to that one lamp already.
GLOW_TEST(a_custom_hostname_is_left_alone) {
  uint8_t mac[6] = {0x98, 0x3d, 0xae, 0x52, 0xc8, 0x2c};
  CHECK(DeviceConfig::uniqueHostname("wohnzimmer", mac) == "wohnzimmer");
  CHECK(DeviceConfig::uniqueHostname("glow-kitchen", mac) == "glow-kitchen");
}

GLOW_TEST(a_derived_hostname_stays_valid_and_bounded) {
  uint8_t mac[6] = {0x98, 0x3d, 0xae, 0x52, 0xc8, 0x2c};

  // Empty falls back to the compiled default and still gets a suffix.
  CHECK(DeviceConfig::uniqueHostname("", mac) == "glowlight-52c82c");

  DeviceConfig config = validConfig();
  config.hostname = DeviceConfig::uniqueHostname(GLOW_HOSTNAME, mac);
  CHECK(config.validate());

  // A long compiled default must not push the name past what a hostname may be;
  // the suffix is what makes it unique, so the base gives way.
  std::string longDefault(60, 'a');
  String derived = DeviceConfig::uniqueHostname(longDefault.c_str(), mac,
                                                longDefault.c_str());
  CHECK_EQ(derived.length(), static_cast<unsigned int>(63));
  CHECK(derived.endsWith("-52c82c"));
  config.hostname = derived;
  CHECK(config.validate());
}

GLOW_TEST(safe_defaults_disable_every_network_service) {
  DeviceConfig safe = DeviceConfig::safeDefaults();

  CHECK(!safe.wifiEnabled);
  CHECK(!safe.communicationEnabled);
  CHECK(!safe.otaEnabled);
  CHECK(!safe.mqttEnabled);
  CHECK(safe.mqttHost.isEmpty());
  CHECK(safe.mqttPassword.isEmpty());
  CHECK(safe.validate());
}

GLOW_TEST(schema_two_configuration_migrates_with_home_assistant_disabled) {
  DeviceConfig config = validConfig();
  JsonDocument legacy;
  CHECK(!deserializeJson(legacy, config.serialize()));
  legacy["schemaVersion"] = 2;
  legacy.remove("mqtt");
  String serialized;
  serializeJson(legacy, serialized);

  DeviceConfig migrated;
  String error;
  CHECK(DeviceConfig::deserialize(serialized, &migrated, &error));
  CHECK(!migrated.mqttEnabled);
  CHECK(migrated.otaPassword == config.otaPassword);
  CHECK(migrated.groupKeyHex == config.groupKeyHex);
}

GLOW_TEST(home_assistant_settings_round_trip_and_are_validated) {
  DeviceConfig config = validConfig();
  config.mqttEnabled = true;
  config.mqttHost = "mqtt.local";
  config.mqttPort = 8883;
  config.mqttUser = "glow";
  config.mqttPassword = "broker-secret";
  config.mqttDiscoveryPrefix = "ha";
  CHECK(config.validate());

  DeviceConfig restored;
  CHECK(DeviceConfig::deserialize(config.serialize(), &restored));
  CHECK(restored.mqttEnabled);
  CHECK(restored.mqttHost == "mqtt.local");
  CHECK_EQ(restored.mqttPort, static_cast<uint16_t>(8883));
  CHECK(restored.mqttUser == "glow");
  CHECK(restored.mqttPassword == "broker-secret");
  CHECK(restored.mqttDiscoveryPrefix == "ha");
}

GLOW_TEST(enabled_home_assistant_rejects_an_unusable_broker_or_missing_wifi) {
  DeviceConfig config = validConfig();
  config.mqttEnabled = true;
  config.mqttHost = "";
  CHECK(!config.validate());

  config.mqttHost = "space in host";
  CHECK(!config.validate());

  config.mqttHost = "mqtt.local";
  config.mqttPort = 0;
  CHECK(!config.validate());

  config.mqttPort = 1883;
  config.mqttDiscoveryPrefix = "";
  CHECK(!config.validate());

  // Without infrastructure WiFi there is no path to a broker at all.
  config.mqttDiscoveryPrefix = "homeassistant";
  config.wifiEnabled = false;
  CHECK(!config.validate());
}

GLOW_TEST(the_broker_password_never_appears_in_a_redacted_configuration) {
  DeviceConfig config = validConfig();
  config.mqttEnabled = true;
  config.mqttHost = "mqtt.local";
  config.mqttPassword = "broker-secret";

  String serialized;
  serializeJson(config.redacted(), serialized);
  CHECK(serialized.indexOf("broker-secret") < 0);
  CHECK(config.redacted()["mqtt"]["passwordConfigured"].as<bool>());
}

GLOW_TEST(enabled_ota_requires_a_separate_strong_password) {
  DeviceConfig config = validConfig();
  config.otaPassword = "short";
  CHECK(!config.validate());
  config.otaPassword = "PROVISION_WITH_SETUP";
  CHECK(!config.validate());
  config.otaEnabled = false;
  CHECK(config.validate());
}

GLOW_TEST(preferences_store_uses_the_newest_valid_generation) {
  glow_shim::resetPreferences();
  PreferencesConfigStore store;
  DeviceConfig first = validConfig();
  DeviceConfig second = validConfig();
  second.hostname = "glow-newer";

  CHECK(store.save(first));
  CHECK(store.save(second));
  DeviceConfig loaded;
  CHECK(store.load(&loaded));
  CHECK(loaded.hostname == "glow-newer");
}

GLOW_TEST(preferences_store_falls_back_when_the_newest_slot_is_corrupt) {
  glow_shim::resetPreferences();
  PreferencesConfigStore store;
  DeviceConfig first = validConfig();
  DeviceConfig second = validConfig();
  second.hostname = "glow-newer";
  CHECK(store.save(first));
  CHECK(store.save(second));

  glow_shim::preferencesValues["glowcfg:slotB"] = "corrupt";
  DeviceConfig loaded;
  CHECK(store.load(&loaded));
  CHECK(loaded.hostname == first.hostname);
}

GLOW_TEST(failed_preferences_write_preserves_the_previous_configuration) {
  glow_shim::resetPreferences();
  PreferencesConfigStore store;
  DeviceConfig first = validConfig();
  CHECK(store.save(first));

  DeviceConfig replacement = validConfig();
  replacement.hostname = "not-written";
  glow_shim::preferencesPutShouldFail = true;
  CHECK(!store.save(replacement));
  glow_shim::preferencesPutShouldFail = false;

  DeviceConfig loaded;
  CHECK(store.load(&loaded));
  CHECK(loaded.hostname == first.hostname);
}

GLOW_TEST(factory_reset_clears_all_persistent_configuration_slots) {
  glow_shim::resetPreferences();
  PreferencesConfigStore store;
  CHECK(store.save(validConfig()));
  CHECK(store.clear());
  DeviceConfig loaded;
  CHECK(!store.load(&loaded));
}
