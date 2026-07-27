#include "LightService.h"


LightService::LightService() {
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(this->currentLeds, LED_NUM_LEDS);
}

void LightService::setup() {
  this->setHardwareBrightness(LED_DEFAULT_BRIGHTNESS);
}

void LightService::loop() {
  bool updated = this->outputDirty;

  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    if (this->currentLeds[i] != this->leds[i]) {
      updated = true;

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

    }
  }

  if (updated) {
    FastLED.show();
    this->outputDirty = false;
  }
}

void LightService::setHardwareBrightness(uint8_t brightness) {
  if (this->hardwareBrightness == brightness) {
    return;
  }

  FastLED.setBrightness(brightness);
  this->hardwareBrightness = brightness;
  this->outputDirty = true;
}

uint8_t LightService::getHardwareBrightness() {
  return this->hardwareBrightness;
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

void LightService::setLedImmediate(uint8_t index, CRGB color) {
  this->leds[index % LED_NUM_LEDS] = color;
  this->currentLeds[index % LED_NUM_LEDS] = color;
  this->outputDirty = true;
}

void LightService::setLedImmediate(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  this->setLedImmediate(index, CRGB(red, green, blue));
}

void LightService::fillImmediate(CRGB color) {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->leds[i] = color;
    this->currentLeds[i] = color;
  }

  this->outputDirty = true;
}
