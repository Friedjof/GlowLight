#ifndef REGISTRY_H
#define REGISTRY_H

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ArrayList.h>
#include <map>
#include <FastLED.h>

#include "GlowTypes.h"
#include "GlowConfig.h"

#include "LinkService.h"


class GlowRegistry {
  private:
    LinkService* linkService;

    String activeNamespace = "";

    JsonDocument _global;
    JsonDocument _private;

    void set(String nameSpace, String key, int value, bool sendUpdate = true);
    int get(String nameSpace, String key);

  public:
    GlowRegistry(LinkService* linkService);
    ~GlowRegistry();

    void setup();
    void loop();

    void set(String key, int value, bool announce = true);
    int get(String key);

    void setPrivateInt(String key, int value);
    void setPrivateU64(String key, uint64_t value);
    void setPrivateStr(String key, String value);

    int getPrivateInt(String key);
    uint64_t getPrivateU64(String key);
    String getPrivateStr(String key);

    void setActive(String nameSpace, bool announce = true);
    String getActive();

    String sync();
    void syncNamespace(String nameSpace, JsonObject data);

    String receive(JsonDocument data);
};

#endif