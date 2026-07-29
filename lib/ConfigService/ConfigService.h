#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "GlowConfig.h"

#ifndef WIFI_ON
#define WIFI_ON false
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef GLOW_HOSTNAME
#define GLOW_HOSTNAME "glowlight"
#endif
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif
#ifndef MESH_ON
#define MESH_ON false
#endif
#ifndef GLOW_GROUP_KEY_HEX
#define GLOW_GROUP_KEY_HEX ""
#endif
#ifndef GLOW_SYNC_FOLLOW_DEFAULT
#define GLOW_SYNC_FOLLOW_DEFAULT true
#endif
#ifndef GLOW_SYNC_PUBLISH_DEFAULT
#define GLOW_SYNC_PUBLISH_DEFAULT true
#endif
#ifndef GLOW_PORTAL_ENABLED
#define GLOW_PORTAL_ENABLED false
#endif
#ifndef GLOW_PORTAL_PASSWORD
#define GLOW_PORTAL_PASSWORD ""
#endif
#ifndef GLOW_OTA_ENABLED
#define GLOW_OTA_ENABLED false
#endif
#ifndef GLOW_OTA_PASSWORD
#define GLOW_OTA_PASSWORD ""
#endif
#ifndef GLOW_MQTT_ENABLED
#define GLOW_MQTT_ENABLED false
#endif
#ifndef GLOW_MQTT_HOST
#define GLOW_MQTT_HOST ""
#endif
#ifndef GLOW_MQTT_PORT
#define GLOW_MQTT_PORT 1883
#endif
#ifndef GLOW_MQTT_USER
#define GLOW_MQTT_USER ""
#endif
#ifndef GLOW_MQTT_PASSWORD
#define GLOW_MQTT_PASSWORD ""
#endif
#ifndef GLOW_MQTT_DISCOVERY_PREFIX
#define GLOW_MQTT_DISCOVERY_PREFIX "homeassistant"
#endif

struct DeviceConfig {
  static constexpr uint16_t SCHEMA_VERSION = 3;

  bool wifiEnabled = WIFI_ON;
  String wifiSsid = WIFI_SSID;
  String wifiPassword = WIFI_PASSWORD;
  String hostname = GLOW_HOSTNAME;
  uint8_t fallbackChannel = ESPNOW_CHANNEL;
  bool communicationEnabled = MESH_ON;
  String groupKeyHex = GLOW_GROUP_KEY_HEX;
  bool syncFollow = GLOW_SYNC_FOLLOW_DEFAULT;
  bool syncPublish = GLOW_SYNC_PUBLISH_DEFAULT;
  bool otaEnabled = GLOW_OTA_ENABLED;
  String otaPassword = GLOW_OTA_PASSWORD;
  bool mqttEnabled = GLOW_MQTT_ENABLED;
  String mqttHost = GLOW_MQTT_HOST;
  uint16_t mqttPort = GLOW_MQTT_PORT;
  String mqttUser = GLOW_MQTT_USER;
  String mqttPassword = GLOW_MQTT_PASSWORD;
  String mqttDiscoveryPrefix = GLOW_MQTT_DISCOVERY_PREFIX;

  static DeviceConfig compileTimeDefaults();
  static DeviceConfig safeDefaults();
  bool validate(String* error = nullptr) const;
  String serialize() const;
  JsonDocument redacted() const;
  static bool deserialize(const String& serialized, DeviceConfig* output,
                          String* error = nullptr);
  static bool fromJson(JsonObjectConst source, const DeviceConfig& current,
                       DeviceConfig* output, String* error = nullptr);

 private:
  static bool validHostname(const String& value);
  static bool validGroupKey(const String& value);
  static bool validWifiPassword(const String& value);
  static bool validOtaPassword(const String& value);
  static bool validMqttHost(const String& value);
};

class ConfigStore {
 public:
  virtual ~ConfigStore() = default;
  virtual bool load(DeviceConfig* output) = 0;
  virtual bool save(const DeviceConfig& config) = 0;
  virtual bool clear() = 0;
};

#endif
