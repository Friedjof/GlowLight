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
    this->lightService->fill(CRGB(255, 230, 160));
  }, false);
  this->addOption("Warm lavender", [this]() {
    this->lightService->fill(CRGB(220, 160, 245));
  }, false);
  this->addOption("Extra warm white", [this]() {
    this->lightService->fill(CRGB(255, 200, 150));
  }, false);
  this->addOption("Warm soft green", [this]() {
    this->lightService->fill(CRGB(140, 200, 140));
  }, false);
  this->addOption("Warmer soft blue", [this]() {
    this->lightService->fill(CRGB(170, 190, 220));
  }, false);
  this->addOption("Warm coral", [this]() {
    this->lightService->fill(CRGB(255, 135, 85));
  }, false);
  this->addOption("Warmer pink", [this]() {
    this->lightService->fill(CRGB(255, 160, 180));
  }, false);
  this->addOption("Gold", [this]() {
    this->lightService->fill(CRGB(255, 200, 50));
  }, false);
  this->addOption("Red", [this]() {
    this->lightService->fill(CRGB(220, 50, 50));
  }, false);
  this->addOption("Lime", [this]() {
    this->lightService->fill(CRGB(100, 255, 100));
  }, false);
  this->addOption("Blue", [this]() {
    this->lightService->fill(CRGB(80, 120, 255));
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
