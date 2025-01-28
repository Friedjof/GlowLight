#include "StaticMode.h"

StaticMode::StaticMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Static Light";
  this->description = "This produces constant light";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void StaticMode::setup() {
  this->setVar("fixed", false);

  this->addOption("Warm soft yellow", [this]() {
    this->fill(CRGB(255, 128, 20));
  }, false);
  this->addOption("Warmer pink", [this]() {
    this->fill(CRGB(255, 180, 200));
  }, false);
  this->addOption("Warm lavender", [this]() {
    this->fill(CRGB(230, 170, 255));
  }, false);
  this->addOption("Extra warm white", [this]() {
    this->fill(CRGB(255, 220, 170));
  }, false);
  this->addOption("Warm soft green", [this]() {
    this->fill(CRGB(160, 220, 160));
  }, false);
  this->addOption("Warmer soft blue", [this]() {
    this->fill(CRGB(190, 210, 240));
  }, false);
  this->addOption("Warm coral", [this]() {
    this->fill(CRGB(255, 155, 105));
  }, false);
  this->addOption("Gold", [this]() {
    this->fill(CRGB(255, 220, 70));
  }, false);
  this->addOption("Red", [this]() {
    this->fill(CRGB(240, 70, 70));
  }, false);
  this->addOption("Lime", [this]() {
    this->fill(CRGB(120, 255, 120));
  }, false);
  this->addOption("Blue", [this]() {
    this->fill(CRGB(100, 140, 255));
  }, false);
}

void StaticMode::customFirst() {
  this->recallCurrentOption();
}

void StaticMode::customLoop() {
  if (!this->getBoolVar("fixed")) {
    this->setBrightness();
  }
}

void StaticMode::fill(CRGB color) {
  this->setVar("color", color);
  this->lightService->fill(color);
}

void StaticMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void StaticMode::customClick() {
  Serial.print("[INFO] " + this->getBoolVar("fixed") ? "Fixed" : "Not fixed");
  this->setVar("fixed", !this->getBoolVar("fixed"));
}
