#include "ColorPickerMode.h"

ColorPickerMode::ColorPickerMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Color Picker";
  this->description = "Color picker mode";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "2.0.0";
  this->license = "MIT";
}

void ColorPickerMode::setup() {
  this->lightService->setBrightness(LED_MAX_BRIGHTNESS);

  this->registry.init("hue", RegistryType::INT, 0, 0, 255);
  this->registry.init("saturation", RegistryType::INT, 255, 0, 255);
  this->registry.init("fixed", RegistryType::BOOL, false);

  this->addOption("Hue", std::function<void()>([this](){ this->newHue(); }));
  this->addOption("Saturation", std::function<void()>([this](){ this->newSaturation(); }));
  this->addOption("Brightness", std::function<void()>([this](){ this->setBrightness(); }));
}

void ColorPickerMode::customFirst() {
  this->lightService->updateLed(CHSV(this->registry.getInt("hue"), this->registry.getInt("saturation"), LED_MAX_BRIGHTNESS));
}

void ColorPickerMode::customLoop() {
  if (this->registry.getBool("fixed")) {
    return;
  }

  this->lightService->updateLed(CHSV(this->registry.getInt("hue"), this->registry.getInt("saturation"), LED_MAX_BRIGHTNESS));
}

void ColorPickerMode::last() {
  // nothing to do
}

void ColorPickerMode::customClick() {
  this->registry.setBool("fixed", !this->registry.getBool("fixed"));
}

bool ColorPickerMode::newHue() {
  if (!this->distanceService->isObjectPresent() || this->registry.getBool("fixed")) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance());

  if (level == this->registry.getInt("hue") || this->distanceService->fixed()) {
    return false;
  }

  this->registry.setInt("hue", level);

  return true;
}

bool ColorPickerMode::newSaturation() {
  if (!this->distanceService->isObjectPresent() || this->registry.getBool("fixed")) {
    return false;
  }

  uint16_t level = this->invExpNormalize(this->getLevel(), 0, DISTANCE_LEVELS, 255, .85);

  if (level == this->registry.getInt("saturation") || this->distanceService->fixed()) {
    return false;
  }

  this->registry.setInt("saturation", level);

  return true;
}

uint16_t ColorPickerMode::distance2hue(uint16_t distance) {
  if (distance < DISTANCE_MIN_MM) {
    return 0;
  } else if (distance > DISTANCE_UNCHANGED_MM) {
    return this->registry.getInt("hue");
  } else if (distance > DISTANCE_MAX_MM) {
    return 255;
  } else {
    return map(distance, DISTANCE_MIN_MM, DISTANCE_MAX_MM, 0, 255);
  }
}

void ColorPickerMode::applyRemoteUpdate(uint16_t distance, uint16_t level) {
  uint8_t currentOption = this->registry.getInt("currentOption");

  if (currentOption == 0) {
    // Option 0: Hue
    // Convert distance to hue (linear mapping)
    uint16_t hue = this->distance2hue(distance);
    this->registry.setInt("hue", hue);

    // Update LED immediately
    this->lightService->updateLed(CHSV(hue, this->registry.getInt("saturation"), LED_MAX_BRIGHTNESS));

    Serial.printf("[DEBUG] Remote update applied: Hue=%d\n", hue);

  } else if (currentOption == 1) {
    // Option 1: Saturation
    // Convert level to saturation
    uint16_t saturation = this->invExpNormalize(level, 0, DISTANCE_LEVELS, 255, 0.85);
    this->registry.setInt("saturation", saturation);

    // Update LED immediately
    this->lightService->updateLed(CHSV(this->registry.getInt("hue"), saturation, LED_MAX_BRIGHTNESS));

    Serial.printf("[DEBUG] Remote update applied: Saturation=%d\n", saturation);

  } else if (currentOption == 2) {
    // Option 2: Brightness
    // Use default implementation (convert level to brightness)
    uint16_t brightness = this->expNormalize(level, 0, DISTANCE_LEVELS, LED_MAX_BRIGHTNESS, 0.5);
    this->lightService->setBrightness(brightness);
    this->brightness = brightness;

    Serial.printf("[DEBUG] Remote update applied: Brightness=%d\n", brightness);
  }
}
