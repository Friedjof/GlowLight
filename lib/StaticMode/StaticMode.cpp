#include "StaticMode.h"

StaticMode::StaticMode(LightService* lightService, DistanceService* distanceService) : AbstractMode(lightService, distanceService) {
  this->title = "Static Light";
  this->description = "This produces constant light";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void StaticMode::setup() {
  this->addOption("Warm soft yellow", [this]() {
    this->lightService->fill(CRGB(255, 240, 180));
  }, false);
  this->addOption("Warm lavender", [this]() {
    this->lightService->fill(CRGB(230, 170, 255));
  }, false);
  this->addOption("Extra warm white", [this]() {
    this->lightService->fill(CRGB(255, 220, 170));
  }, false);
  this->addOption("Warm soft green", [this]() {
    this->lightService->fill(CRGB(160, 220, 160));
  }, false);
  this->addOption("Warmer soft blue", [this]() {
    this->lightService->fill(CRGB(190, 210, 240));
  }, false);
  this->addOption("Warm coral", [this]() {
    this->lightService->fill(CRGB(255, 155, 105));
  }, false);
  this->addOption("Warmer pink", [this]() {
    this->lightService->fill(CRGB(255, 180, 200));
  }, false);
  this->addOption("Gold", [this]() {
    this->lightService->fill(CRGB(255, 220, 70));
  }, false);
  this->addOption("Red", [this]() {
    this->lightService->fill(CRGB(240, 70, 70));
  }, false);
  this->addOption("Lime", [this]() {
    this->lightService->fill(CRGB(120, 255, 120));
  }, false);
  this->addOption("Blue", [this]() {
    this->lightService->fill(CRGB(100, 140, 255));
  }, false);
}

void StaticMode::customFirst() {
  this->recallCurrentOption();
}

void StaticMode::customLoop() {
  this->setBrightness();
}

void StaticMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void StaticMode::customClick() {
  // nothing to do
}
