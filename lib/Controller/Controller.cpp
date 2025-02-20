#include "Controller.h"

Controller::Controller(DistanceService* distanceService, CommunicationService* communicationService) {
  this->distanceService = distanceService;
  this->communicationService = communicationService;
}

// mode functions
void Controller::addMode(AbstractMode* mode) {
  Serial.print("[INFO] Added mode '");
  Serial.print(mode->getTitle());
  Serial.println("'");

  // mode setup function
  mode->modeSetup();

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

  this->event();
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

// option functions
void Controller::nextOption() {
  bool alertEnabled = this->currentMode->nextOption();

  this->event();

  if (alertEnabled) {
    this->enableAlert(2);
  }
}

void Controller::setOption(uint8_t option) {
  bool alertEnabled = this->currentMode->setOption(option);

  if (alertEnabled) {
    this->enableAlert(2);
  }
}

// custom click function
void Controller::customClick() {
  Serial.println("[DEBUG] Custom click");

  this->currentMode->customClick();

  this->event();
}

// main functions
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
  this->communicationService->onReceived(std::bind(&Controller::newMessageCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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

  if (this->distanceService->hasObjectDisappeared()) {
    this->event();
  }
}

// alert functions
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

// communication functions
void Controller::newConnectionCallback() {
  this->enableAlert(4, CRGB(0, 255, 0));

  this->communicationService->sendSync(millis());
}

void Controller::newMessageCallback(uint32_t from, JsonDocument message, MessageType type) {
  if (type == MessageType::EVENT) {
    // the EVENT message will be triggered if a change on another node is detected

    // check if the event has the correct format
    if (!message["title"].is<String>() || !message["version"].is<String>()) {
      Serial.println("[ERROR] Invalid message event format, ignoring message");
      return;
    }

    Serial.println("[DEBUG] Event message received");

    // check if the mode has changed
    if (message["title"].as<String>() != this->currentMode->getTitle()) {
      this->setMode(message["title"].as<String>());
    }

    // deserialize the event
    this->currentMode->deserialize(message);
  } else if (type == MessageType::SYNC) {
    /* The SYNC message will be triggered if a new node is detected:
     * - 'timestamp' holds the current value from the sender GlowNode
     * - The node with the highest timestamp will send the current state to the other GlowNode
     * - In the case of equal timestamps, no action will be taken (very unlikely)
     */

    // check if the sync has the correct format
    if (!message["timestamp"].is<uint64_t>()) {
      Serial.println("[ERROR] Invalid message sync format, ignoring message");
      return;
    }

    Serial.println("[DEBUG] Sync message received");

    // if the new GlowNode is younger, it will send the current state
    if (message["timestamp"].as<uint64_t>() < millis()) {
      Serial.println("[DEBUG] this GlowNode is older and will send the current state");
      this->event();
    } else {
      Serial.println("[DEBUG] this GlowNode will not send the current state");
    }

  } else {
    Serial.println("[ERROR] Invalid message type, ignoring message");
  }
}

void Controller::event() {
  this->communicationService->sendEvent(this->currentMode->serialize());
}
