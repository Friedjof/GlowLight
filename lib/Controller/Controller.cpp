#include "Controller.h"

Controller::Controller(DistanceService* distanceService) {
  this->distanceService = distanceService;
}

void Controller::addMode(AbstractMode* mode) {
  Serial.print("[INFO] Added mode '");
  Serial.print(mode->getTitle());
  Serial.println("'");

  // setup mode
  mode->setup();

  // add mode to mode list
  this->modes.add(mode);
}

void Controller::setAlertMode(Alert* mode) {
  this->alertMode = mode;
}

void Controller::nextMode() {
  if (this->currentMode != nullptr) {
    this->currentMode->last();
  }

  AbstractMode* mode = this->currentMode;

  if (++this->currentModeIndex >= this->modes.size()) {
    this->currentModeIndex = 0;
  }

  this->currentMode = this->modes.get(this->currentModeIndex);

  if (this->currentMode == nullptr) {
    Serial.println("[ERROR] nextMode - Mode is null");
    return;
  }

  this->previousMode = mode;

  Serial.print("[INFO] Switched to mode '");
  Serial.print(this->currentMode->getTitle());
  Serial.print("' by '");
  Serial.print(this->currentMode->getAuthor());
  Serial.println("'");

  this->currentMode->first();
}

void Controller::nextOption() {
  bool alertEnabled = this->currentMode->nextOption();

  if (alertEnabled) {
    this->enableAlert(2);
  }
}

void Controller::customClick() {
  Serial.println("[DEBUG] Custom click");

  this->currentMode->customClick();
}

void Controller::setup() {
  if (this->alertMode == nullptr) {
    Serial.println("[ERROR] Alert mode is null");
    return;
  }

  if (this->modes.size() == 0) {
    Serial.println("[ERROR] No modes added");
    return;
  }

  this->enableAlert(5);
}

void Controller::loop() {
  if (this->modes.size() == 0) {
    return;
  }

  if (this->currentMode == nullptr) {
    Serial.println("[ERROR] loop - Mode is null");
    return;
  }

  if (this->distanceService->alert() && !this->alertEnabled()) {
    this->enableAlert(2);
  }

  this->currentMode->loop();

  if (this->alertEnabled() && !this->alertMode->isFlashing()) {
    this->disableAlert();
  }
}

void Controller::enableAlert(uint8_t flashes) {
  if (this->currentMode == this->alertMode) {
    return;
  }

  if (this->currentMode != nullptr) {
    this->currentMode->last();
  }

  if (this->alertMode == nullptr) {
    Serial.println("[ERROR] Alert mode is null");
    return;
  }

  if (this->currentMode != nullptr) {
    this->previousMode = this->currentMode;
  }

  this->currentMode = this->alertMode;

  this->alertMode->setFlashes(flashes);
  this->alertMode->first();

  Serial.print("[INFO] Switched to alert mode '");
  Serial.print(this->alertMode->getTitle());
  Serial.print("' by '");
  Serial.print(this->alertMode->getAuthor());
  Serial.println("'");
}

void Controller::disableAlert() {
  if (!this->alertEnabled()) {
    return;
  }

  if (this->previousMode == nullptr) {
    if (this->modes.size() > 0) {
      this->previousMode = this->modes.get(0);
    } else {
      Serial.println("[ERROR] No previous mode, cannot disable alert");
      return;
    }
  }

  this->currentMode = this->previousMode;
  this->previousMode = this->alertMode;

  this->currentMode->first();

  Serial.print("[INFO] Switched to mode '");
  Serial.print(this->currentMode->getTitle());
  Serial.print("' by '");
  Serial.print(this->currentMode->getAuthor());
  Serial.println("'");
}

bool Controller::alertEnabled() {
  return this->currentMode == this->alertMode;
}