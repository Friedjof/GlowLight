#ifndef REGISTRY_H
#define REGISTRY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArrayList.h>
#include <FastLED.h>


enum class RegistryType {
    INT,
    STRING,
    BOOL,
    COLOR
};


bool operator==(RegistryType a, RegistryType b) {
    return static_cast<int>(a) == static_cast<int>(b);
}


class GlowRegistry {
  private:
    JsonDocument registry;
    JsonDocument meta;
    ArrayList<String> keys;

    // helper functions
    String CRGB2Hex(CRGB color);
    CRGB Hex2CRGB(String hex);

    uint16_t type2int(RegistryType type);
    RegistryType int2type(uint16_t type);

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
    bool remove(String key);

    // serialize and deserialize
    JsonDocument serialize();
    bool deserialize(JsonDocument doc);
};

#endif