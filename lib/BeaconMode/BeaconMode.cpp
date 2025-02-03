#include "BeaconMode.h"

BeaconMode::BeaconMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Beacon";
  this->description = "This mode simulates a beacon";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "2.0.0";
  this->license = "MIT";
}

void BeaconMode::setup() {
  this->registry.init("hueOne", RegistryType::INT, 0, 0, 255);
  this->registry.init("hueTwo", RegistryType::INT, 192, 0, 255);
  this->registry.init("speed", RegistryType::INT, BEACON_SPEED_DEFAULT, BEACON_SPEED_MIN, BEACON_SPEED_MAX);

  this->addOption("Speed", [this]() {
    this->newSpeed();
  }, true);
  this->addOption("Hue one", [this]() {
    this->newHueOne();
  }, true);
  this->addOption("Hue two", [this]() {
    this->newHueTwo();
  }, true);
  this->addOption("Brightness", std::function<void()>([this](){
    this->setBrightness();
  }));
}

void BeaconMode::customFirst() {
  this->counter = 0;

  this->recallCurrentOption();
}

void BeaconMode::customLoop() {
  if (this->counter++ % this->registry.getInt("speed") == 0) {
    this->setHue(this->position, this->registry.getInt("hueOne"));

    this->position = (this->position + 1) % LED_NUM_LEDS;

    this->setHue((this->position + BEACON_LENGTH_DEFAULT) % LED_NUM_LEDS, this->registry.getInt("hueTwo"));
  }
}

void BeaconMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void BeaconMode::customClick() {
  this->smoothTransition = !this->smoothTransition;
}

bool BeaconMode::newSpeed() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  uint16_t spd = this->expNormalize(level, 0, DISTANCE_LEVELS, BEACON_SPEED_MIN, BEACON_SPEED_MAX);

  if (spd == this->registry.getInt("speed")) {
    return false;
  }

  this->registry.setInt("speed", spd);

  return true;
}

bool BeaconMode::newHueOne() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance(), this->registry.getInt("hueOne"));

  if (level == this->registry.getInt("hueOne")) {
    return false;
  }

  this->registry.setInt("hueOne", level);

  return true;
}

bool BeaconMode::newHueTwo() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  uint16_t level = this->distance2hue(this->getDistance(), this->registry.getInt("hueTwo"));

  if (level == this->registry.getInt("hueTwo")) {
    return false;
  }

  this->registry.setInt("hueTwo", level);

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
