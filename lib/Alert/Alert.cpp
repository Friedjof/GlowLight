#include "Alert.h"

Alert::Alert(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Alert";
  this->description = "Flashing alert mode";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void Alert::setup() {
  this->lightService->setBrightness(0);
  this->lightService->fill(this->color);
}

void Alert::customFirst() {
  this->flashing = true;
  this->index = 0;

  this->lightService->fill(this->color);
}

void Alert::customLoop() {
  if (!this->flashing) {
    return;
  }

  this->updateBrightness(this->index % (LED_MAX_BRIGHTNESS * 2 - 1) < LED_MAX_BRIGHTNESS ? 
    this->index % LED_MAX_BRIGHTNESS : 
    LED_MAX_BRIGHTNESS - (this->index % LED_MAX_BRIGHTNESS));

  this->index += ALERT_SPEED_STEP;

  if (this->index > LED_MAX_BRIGHTNESS * this->flashes) {
    this->flashing = false;
  }
}

void Alert::last() {
  // nothing to do
}

void Alert::customClick() {
  // nothing to do
}

bool Alert::isFlashing() {
  return this->flashing;
}

bool Alert::setFlashes(uint8_t flashes) {
  if (flashes == 0) {
    this->flashing = false;
    return true;
  }
  
  if (this->flashes != flashes) {
    this->flashes = flashes;
    return true;
  }

  return false;
}

bool Alert::setColor(CRGB color) {
  if (this->color != color) {
    this->color = color;
    return true;
  }

  return false;
}

CRGB Alert::getColor() {
  return this->color;
}