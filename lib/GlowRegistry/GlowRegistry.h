/*
 * GlowRegistry.h - Typed mode state plus machine-readable setting metadata.
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArrayList.h>
#include <FastLED.h>


enum RegistryType {
    INT = 0,
    STRING = 1,
    BOOL = 2,
    COLOR = 3
};


class GlowRegistry {
  private:
    JsonDocument registry;
    JsonDocument meta;

    // helper functions
    String CRGB2Hex(CRGB color);
    CRGB Hex2CRGB(String hex);
    bool isHexColor(const String& hex);
    // Accepts the spellings a human or a UI actually produces — surrounding
    // whitespace and a leading '#' — and returns the bare six hex digits, or an
    // empty string when the value is not a colour. Normalising here rather than
    // in one adapter means every path benefits: MQTT, the control API, the
    // serial console and the ESP-NOW group sync.
    String normalizeHexColor(const String& value);
    bool valueValid(const String& key, JsonVariantConst value);
    bool applyValue(const String& key, JsonVariantConst value);

  public:
    GlowRegistry();

    // meta functions
    void setTitle(String title);
    void setVersion(String version);
    String getTitle();
    String getVersion();
    bool hasTitle();
    bool hasVersion();

    // init functions
    bool init(String key, RegistryType type);
    bool init(String key, RegistryType type, uint16_t defaultValue);
    bool init(String key, RegistryType type, uint16_t defaultValue, uint16_t min, uint16_t max);
    bool init(String key, RegistryType type, String defaultValue);
    bool init(String key, RegistryType type, bool defaultValue);
    bool init(String key, RegistryType type, CRGB defaultValue);

    // get functions
    uint16_t getInt(String key);
    String getString(String key);
    bool getBool(String key);
    CRGB getColor(String key);

    // set functions
    bool setInt(String key, uint16_t value);
    bool setString(String key, String value);
    bool setBool(String key, bool value);
    bool setColor(String key, CRGB value);

    // other functions
    bool reset(String key);
    uint16_t size();
    bool contains(String key);
    bool setWritable(String key, bool writable);
    bool setValue(String key, JsonVariantConst value);

    // serialize and deserialize
    JsonDocument serialize();
    JsonDocument describe();
    bool deserialize(const JsonDocument& doc);
};

#endif
