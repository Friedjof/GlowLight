#include "LightService.h"


LightService::LightService() {
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(this->currentLeds, LED_NUM_LEDS);
}

void LightService::setup() {
  this->setBrightness(LED_MAX_BRIGHTNESS);
}

void LightService::loop() {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    if (this->currentLeds[i] != this->leds[i]) {
      if (this->currentLeds[i].r < this->leds[i].r) {
        if (this->currentLeds[i].r + this->lightUpdateSteps < this->leds[i].r) {
          this->currentLeds[i].r += this->lightUpdateSteps;
        } else {
          this->currentLeds[i].r = this->leds[i].r;
        }
      } else if (this->currentLeds[i].r > this->leds[i].r) {
        if (this->currentLeds[i].r - this->lightUpdateSteps > this->leds[i].r) {
          this->currentLeds[i].r -= this->lightUpdateSteps;
        } else {
          this->currentLeds[i].r = this->leds[i].r;
        }
      }

      if (this->currentLeds[i].g < this->leds[i].g) {
        if (this->currentLeds[i].g + this->lightUpdateSteps < this->leds[i].g) {
          this->currentLeds[i].g += this->lightUpdateSteps;
        } else {
          this->currentLeds[i].g = this->leds[i].g;
        }
      } else if (this->currentLeds[i].g > this->leds[i].g) {
        if (this->currentLeds[i].g - this->lightUpdateSteps > this->leds[i].g) {
          this->currentLeds[i].g -= this->lightUpdateSteps;
        } else {
          this->currentLeds[i].g = this->leds[i].g;
        }
      }

      if (this->currentLeds[i].b < this->leds[i].b) {
        if (this->currentLeds[i].b + this->lightUpdateSteps < this->leds[i].b) {
          this->currentLeds[i].b += this->lightUpdateSteps;
        } else {
          this->currentLeds[i].b = this->leds[i].b;
        }
      } else if (this->currentLeds[i].b > this->leds[i].b) {
        if (this->currentLeds[i].b - this->lightUpdateSteps > this->leds[i].b) {
          this->currentLeds[i].b -= this->lightUpdateSteps;
        } else {
          this->currentLeds[i].b = this->leds[i].b;
        }
      }

      FastLED.show();
    }
  }
}

void LightService::setBrightness(uint8_t brightness) {
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void LightService::setLightUpdateSteps(uint16_t steps) {
  this->lightUpdateSteps = steps;
}

void LightService::fill(uint8_t red, uint8_t green, uint8_t blue) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->leds[i] = CRGB(red, green, blue);
  }
}

void LightService::fill(uint32_t color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->leds[i] = color;
  }
}

void LightService::fill(CRGB color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->leds[i] = color;
  }
}

void LightService::setLed(uint8_t index, CRGB color) {
  this->leds[index % LED_NUM_LEDS] = color;
}

void LightService::setLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  this->setLed(index, CRGB(red, green, blue));
}

void LightService::setLed(CRGB color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->setLed(i, color);
  }
}

void LightService::updateLed(uint8_t index, CRGB color) {
  this->leds[index % LED_NUM_LEDS] = color;
  this->currentLeds[index % LED_NUM_LEDS] = color;

  FastLED.show();
}

void LightService::updateLed(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  this->updateLed(index, CRGB(red, green, blue));
}

void LightService::updateLed(CRGB color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->leds[i] = color;
    this->currentLeds[i] = color;
  }

  FastLED.show();
}

void LightService::show() {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    currentLeds[i] = leds[i];
  }

  FastLED.show();
}