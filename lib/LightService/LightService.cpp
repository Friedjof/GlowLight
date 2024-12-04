#include "LightService.h"


LightService::LightService() {
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, LED_NUM_LEDS);
}

void LightService::setup() {
  this->setBrightness(LED_MAX_BRIGHTNESS);
}

void LightService::setBrightness(uint8_t brightness) {
  FastLED.setBrightness(brightness);
  this->show();
}

void LightService::fill(uint8_t red, uint8_t green, uint8_t blue) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    leds[i] = CRGB(red, green, blue);
  }
  this->show();
}

void LightService::fill(uint32_t color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    leds[i] = color;
  }
  this->show();
}

void LightService::fill(CRGB color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    leds[i] = color;
  }
  this->show();
}

void LightService::setLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  leds[index] = CRGB(red, green, blue);
}

void LightService::setLed(uint8_t index, uint32_t color) {
  leds[index] = color;
}

void LightService::setLed(uint8_t index, CRGB color) {
  leds[index] = color;
}

void LightService::show() {
  FastLED.show();
}