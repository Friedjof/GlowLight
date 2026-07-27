#ifndef LIGHTSERVICE_H
#define LIGHTSERVICE_H

#include <Arduino.h>
#include <FastLED.h>
#include "GlowConfig.h"


class LightService {
  private:
    CRGB leds[LED_NUM_LEDS];
    CRGB currentLeds[LED_NUM_LEDS];

    uint16_t lightUpdateSteps = LED_UPDATE_STEPS;
    uint8_t hardwareBrightness = LED_MAX_BRIGHTNESS;
    bool outputDirty = false;

  public:
    LightService();

    void setup();
    void loop();

    void setHardwareBrightness(uint8_t brightness);
    uint8_t getHardwareBrightness();

    void setLightUpdateSteps(uint16_t steps);

    void fill(uint8_t red, uint8_t green, uint8_t blue);
    void fill(uint32_t color);
    void fill(CRGB color);

    void setLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    void setLed(uint8_t index, CRGB color);
    void setLedImmediate(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    void setLedImmediate(uint8_t index, CRGB color);
    void fillImmediate(CRGB color);
};

#endif
