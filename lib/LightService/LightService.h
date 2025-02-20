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
    uint8_t brightness = LED_DEFAULT_BRIGHTNESS;

  public:
    LightService();

    void setup();
    void loop();

    void show();

    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();

    void setLightUpdateSteps(uint16_t steps);

    void fill(uint8_t red, uint8_t green, uint8_t blue);
    void fill(uint32_t color);
    void fill(CRGB color);

    void setLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    void setLed(uint8_t index, CRGB color);
    void setLed(CRGB color);

    void updateLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    void updateLed(uint8_t index, CRGB color);
    void updateLed(CRGB color);
};

#endif
