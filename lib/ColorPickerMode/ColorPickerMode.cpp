#include "ColorPickerMode.h"

ColorPickerMode::ColorPickerMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Color Picker";
  this->description = "Color picker mode";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void ColorPickerMode::setup() {
  this->lightService->setBrightness(LED_MAX_BRIGHTNESS);

  this->addOption("Hue", std::function<void()>([this](){ this->newHue(); }));
  this->addOption("Saturation", std::function<void()>([this](){ this->newSaturation(); }));
  this->addOption("Brightness", std::function<void()>([this](){ this->setBrightness(); }));
}

void ColorPickerMode::customFirst() {
  this->lightService->updateLed(CHSV(this->hue, this->saturation, LED_MAX_BRIGHTNESS));
}

void ColorPickerMode::customLoop() {
  if (this->fixed) {
    return;
  }

  this->lightService->updateLed(CHSV(this->hue, this->saturation, LED_MAX_BRIGHTNESS));
}

void ColorPickerMode::last() {
  // nothing to do
}

void ColorPickerMode::customClick() {
  this->fixed = !this->fixed;
}

bool ColorPickerMode::newHue() {
  if (!this->distanceService->isObjectPresent() || this->fixed) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance());

  if (level == this->hue || this->distanceService->fixed()) {
    return false;
  }

  this->hue = level;

  return true;
}

bool ColorPickerMode::newSaturation() {
  if (!this->distanceService->isObjectPresent() || this->fixed) {
    return false;
  }

  uint16_t level = this->invExpNormalize(this->getLevel(), 0, DISTANCE_LEVELS, 255, .85);

  if (level == this->saturation) {
    return false;
  }

  this->saturation = level;

  return true;
}

uint16_t ColorPickerMode::distance2hue(uint16_t distance) {
  if (distance < DISTANCE_MIN_MM) {
    return 0;
  } else if (distance > DISTANCE_UNCHANGED_MM) {
    return this->hue;
  } else if (distance > DISTANCE_MAX_MM) {
    return 255;
  } else {
    return map(distance, DISTANCE_MIN_MM, DISTANCE_MAX_MM, 0, 255);
  }
}
