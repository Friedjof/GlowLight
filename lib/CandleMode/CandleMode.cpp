#include "CandleMode.h"

CandleMode::CandleMode(LightService* lightService, DistanceService* distanceService) : AbstractMode(lightService, distanceService) {
  this->title = "Candle Light";
  this->description = "This produces a candle light effect";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void CandleMode::setup() {
  this->colors.add(CRGB(255, 63,  0));  // deep fire red
  this->colors.add(CRGB(255, 87,  17)); // glowing ember
  this->colors.add(CRGB(255, 47,  0));  // intense flame red
  this->colors.add(CRGB(255, 95,  35)); // molten glow
  this->colors.add(CRGB(255, 72,  20)); // fiery crimson

  this->addOption("Brightness", std::function<void()>([this](){ this->setBrightness(); }));
  this->addOption("Speed", std::function<void()>([this](){ this->newSpeed(); }));
}

void CandleMode::customFirst() {
  // nothing to do
}

void CandleMode::customLoop() {
  if (this->optionHasChanged()) {
    if (this->getCurrentOption() == 0) {
      Serial.println("[INFO] Selected option 'Brightness'");
    } else if (this->getCurrentOption() == 1) {
      Serial.println("[INFO] Selected option 'Speed'");
    }
  }

  if (millis() % this->speed == 0) {
    for (uint8_t i = 0; i < LED_NUM_LEDS; i++) {
      this->lightService->setLed(i, this->colors.get(random(0, this->colors.size())));
    }
    this->lightService->show();
  }
}

void CandleMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void CandleMode::customClick() {
  // nothing to do
}

bool CandleMode::newSpeed() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  uint16_t spd = this->expNormalize(level, 0, DISTANCE_LEVELS, CANDLE_SPEED_MAX, .5);

  if (spd == this->speed) {
    return false;
  }

  this->speed = spd;

  return true;
}