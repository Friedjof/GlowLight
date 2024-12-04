#ifndef LIGHTSERVICE_H
#define LIGHTSERVICE_H

#include <Arduino.h>
#include <FastLED.h>
#include "GlowConfig.h"


class LightService {
  private:
    CRGB leds[LED_NUM_LEDS];

  public:
    LightService();

    void setup();

    void show();

    void setBrightness(uint8_t brightness);

    void fill(uint8_t red, uint8_t green, uint8_t blue);
    void fill(uint32_t color);
    void fill(CRGB color);

    void setLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    void setLed(uint8_t index, uint32_t color);
    void setLed(uint8_t index, CRGB color);
};

#endif
