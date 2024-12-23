#include "BeaconMode.h"

BeaconMode::BeaconMode(LightService* lightService, DistanceService* distanceService) : AbstractMode(lightService, distanceService) {
  this->title = "Beacon";
  this->description = "This mode simulates a beacon";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void BeaconMode::setup() {
  this->addOption("Speed", [this]() {
    this->newSpeed();
  }, true);
  this->addOption("Hue one", [this]() {
    this->newHueOne();
  }, true);
  this->addOption("Hue two", [this]() {
    this->newHueTwo();
  }, true);
}

void BeaconMode::customFirst() {
  this->counter = 0;

  this->recallCurrentOption();
}

void BeaconMode::customLoop() {
  if (this->counter++ % this->speed == 0) {
    this->setHue(this->position, this->hueOne);

    this->position = (this->position + 1) % LED_NUM_LEDS;

    for (int i = 0; i < BEACON_LENGTH_DEFAULT; i++) {
      this->setHue(this->position + i, this->hueTwo);
    }
  }
}

void BeaconMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void BeaconMode::customClick() {
  this->smoothTransition = !this->smoothTransition;
}

bool BeaconMode::newSpeed() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  uint16_t spd = this->expNormalize(level, 0, DISTANCE_LEVELS, BEACON_SPEED_MIN, BEACON_SPEED_MAX);

  if (spd == this->speed) {
    return false;
  }

  this->speed = spd;

  return true;
}

bool BeaconMode::newHueOne() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance(), this->hueOne);

  if (level == this->hueOne) {
    return false;
  }

  this->hueOne = level;

  return true;
}

bool BeaconMode::newHueTwo() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance(), this->hueTwo);

  if (level == this->hueTwo) {
    return false;
  }

  this->hueTwo = level;

  return true;
}

uint16_t BeaconMode::distance2hue(uint16_t distance, uint16_t currentHue) {
  if (distance < DISTANCE_MIN_MM) {
    return 0;
  } else if (distance > DISTANCE_UNCHANGED_MM) {
    return currentHue;
  } else if (distance > DISTANCE_MAX_MM) {
    return 255;
  } else {
    return map(distance, DISTANCE_MIN_MM, DISTANCE_MAX_MM, 0, 255);
  }
}

void BeaconMode::setHue(uint16_t index, uint8_t hue) {
  if (this->smoothTransition) {
    this->lightService->setLed(index, CHSV(hue, 255, 255));
  } else {
    this->lightService->updateLed(index, CHSV(hue, 255, 255));
  }
}
