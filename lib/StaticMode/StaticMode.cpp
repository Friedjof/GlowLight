#include "StaticMode.h"

StaticMode::StaticMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Static Light";
  this->id = "static";
  this->description = "This produces constant light";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void StaticMode::setup() {
  this->registry.init("color", RegistryType::COLOR, CRGB(255, 128, 20));
  this->registry.init("fixed", RegistryType::BOOL, false);

  this->addOption("Warm soft yellow", [this]() {
    this->fill(CRGB(255, 128, 20));
  }, false, true);
  this->addOption("Warmer pink", [this]() {
    this->fill(CRGB(255, 180, 200));
  }, false, true);
  this->addOption("Warm lavender", [this]() {
    this->fill(CRGB(230, 170, 255));
  }, false, true);
  this->addOption("Extra warm white", [this]() {
    this->fill(CRGB(255, 220, 170));
  }, false, true);
  this->addOption("Warm soft green", [this]() {
    this->fill(CRGB(160, 220, 160));
  }, false, true);
  this->addOption("Warmer soft blue", [this]() {
    this->fill(CRGB(190, 210, 240));
  }, false, true);
  this->addOption("Warm coral", [this]() {
    this->fill(CRGB(255, 155, 105));
  }, false, true);
  this->addOption("Gold", [this]() {
    this->fill(CRGB(255, 220, 70));
  }, false, true);
  this->addOption("Red", [this]() {
    this->fill(CRGB(240, 70, 70));
  }, false, true);
  this->addOption("Lime", [this]() {
    this->fill(CRGB(120, 255, 120));
  }, false, true);
  this->addOption("Blue", [this]() {
    this->fill(CRGB(100, 140, 255));
  }, false, true);
}

void StaticMode::customFirst() {
  this->lightService->fill(this->registry.getColor("color"));
}

void StaticMode::customLoop() {
  if (!this->registry.getBool("fixed")) {
    this->updateBrightnessFromSensor();
  }
}

void StaticMode::fill(CRGB color) {
  this->registry.setColor("color", color);
  this->lightService->fill(color);
}

void StaticMode::last() {
  Serial.println("[INFO] Deselected mode '" + this->getTitle() + "'");
}

void StaticMode::customClick() {
  Serial.print("[INFO] " + this->registry.getBool("fixed") ? "Fixed" : "Not fixed");
  this->registry.setBool("fixed", !this->registry.getBool("fixed"));
}

void StaticMode::onStateApplied() {
  this->fixed = this->registry.getBool("fixed");
  this->lightService->fill(this->registry.getColor("color"));
}

void StaticMode::onSettingChanged(const String& key) {
  if (key == "color") this->lightService->fill(this->registry.getColor("color"));
}

void StaticMode::onStateActivated() {
  this->lightService->fill(this->registry.getColor("color"));
}
