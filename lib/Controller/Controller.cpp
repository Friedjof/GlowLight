#include "Controller.h"

Controller::Controller(DistanceService* distanceService, CommunicationService* communicationService) {
  this->distanceService = distanceService;
  this->communicationService = communicationService;
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

void Controller::printSwitchedMode(AbstractMode* mode) {
  Serial.print("[INFO] Switched to mode '");
  Serial.print(mode->getTitle());
  Serial.print("' by '");
  Serial.print(mode->getAuthor());
  Serial.println("'");
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

  this->printSwitchedMode(this->currentMode);

  this->currentMode->first();

  this->communicationService->sendSyncMessage(this->currentMode->getTitle(), this->currentMode->getCurrentOption(), this->currentMode->getBrightness());
}

void Controller::setMode(String title) {
  if (this->currentMode != nullptr) {
    this->currentMode->last();
  }

  AbstractMode* mode = this->currentMode;

  for (int i = 0; i < this->modes.size(); i++) {
    if (this->modes.get(i)->getTitle() == title) {
      this->currentMode = this->modes.get(i);
      this->currentModeIndex = i;

      this->previousMode = mode;

      this->printSwitchedMode(this->currentMode);

      this->currentMode->first();

      return;
    }
  }

  Serial.print("[ERROR] Mode '");
  Serial.print(title);
  Serial.println("' not found");
}

void Controller::nextOption() {
  bool alertEnabled = this->currentMode->nextOption();

  if (alertEnabled) {
    this->enableAlert(2);
  }

  this->communicationService->sendSyncMessage(this->currentMode->getTitle(), this->currentMode->getCurrentOption(), this->currentMode->getBrightness());
}

void Controller::setOption(uint8_t option) {
  bool alertEnabled = this->currentMode->setOption(option);

  if (alertEnabled) {
    this->enableAlert(2);
  }

  this->communicationService->sendSyncMessage(this->currentMode->getTitle(), this->currentMode->getCurrentOption(), this->currentMode->getBrightness());
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

  this->communicationService->onNewConnection(std::bind(&Controller::newConnectionCallback, this));
  this->communicationService->onReceived(std::bind(&Controller::newMessageCallback, this, std::placeholders::_1, std::placeholders::_2));

  Serial.println("[INFO] Controller initialized");

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

void Controller::enableAlert(uint8_t flashes, CRGB color) {
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

  this->alertMode->setColor(color);
  this->alertMode->setFlashes(flashes);
  this->alertMode->first();

  Serial.print("[INFO] Switched to alert mode '");
  Serial.print(this->alertMode->getTitle());
  Serial.print("' by '");
  Serial.print(this->alertMode->getAuthor());
  Serial.println("'");
}

void Controller::enableAlert(uint8_t flashes) {
  this->enableAlert(flashes, CRGB(255, 128, 20));
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

void Controller::newConnectionCallback() {
  this->enableAlert(4, CRGB(0, 255, 0));
}

void Controller::newMessageCallback(uint32_t from, String message) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("[ERROR] deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc["type"] == "sync") {
    String mode = doc["mode"];
    uint16_t option = doc["option"];
    uint16_t brightness = doc["brightness"];

    this->setMode(mode);
    this->setOption(option);
    this->currentMode->updateBrightness(brightness);

  } else if (doc["type"] == "brightness") {
    uint16_t brightness = doc["brightness"];

    this->currentMode->updateBrightness(brightness);
  }
}