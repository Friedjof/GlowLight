#include "RainbowMode.h"

RainbowMode::RainbowMode(LightService* lightService, DistanceService* distanceService) : AbstractMode(lightService, distanceService) {
  this->title = "Rainbow";
  this->description = "Rainbow mode";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void RainbowMode::setup() {
  this->lightService->setBrightness(LED_MAX_BRIGHTNESS);

  this->addOption("Brightness", std::function<void()>([this](){ this->setBrightness(); }));
  this->addOption("Saturation", std::function<void()>([this](){ this->newSaturation(); }));
  this->addOption("Speed", std::function<void()>([this](){ this->newSpeed(); }));
}

void RainbowMode::customFirst() {
  // nothing to do
}

void RainbowMode::customLoop() {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    int hue = map((i + this->index) % LED_NUM_LEDS, 0, LED_NUM_LEDS, 0, 255);

    this->lightService->setLed(i, CHSV(hue, this->saturation, LED_MAX_BRIGHTNESS));
  }

  if (this->counter++ % this->speed == 0 && !this->stopped) {
    if (++this->index > LED_NUM_LEDS) {
      this->index = 0;
    }
  }
}

void RainbowMode::last() {
  // nothing to do
}

void RainbowMode::customClick() {
  this->stopped = !this->stopped;
}

bool RainbowMode::newSaturation() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->invExpNormalize(this->getLevel(), 0, DISTANCE_LEVELS, 255, .85);

  if (level == this->saturation) {
    return false;
  }

  this->saturation = level;

  return true;
}

bool RainbowMode::newSpeed() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  uint16_t spd = this->expNormalize(level, 0, DISTANCE_LEVELS, RAINBOW_SPEED_MIN - RAINBOW_SPEED_MAX, .5) + RAINBOW_SPEED_MAX;

  if (spd == this->speed) {
    return false;
  }

  this->speed = spd;

  return true;
}
