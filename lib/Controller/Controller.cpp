#include "Controller.h"

Controller::Controller(DistanceService* distanceService, CommunicationService* communicationService) {
  this->distanceService = distanceService;
  this->communicationService = communicationService;
}

// mode functions
void Controller::addMode(AbstractMode* mode) {
  if (mode == nullptr) {
    Serial.println("[ERROR] Cannot add a null mode");
    return;
  }

  Serial.print("[INFO] Added mode '");
  Serial.print(mode->getTitle());
  Serial.println("'");

  // mode setup function
  mode->modeSetup();

  // add mode to mode list
  this->modes.add(mode);
}

void Controller::setAlertMode(Alert* mode) {
  if (mode == nullptr) {
    Serial.println("[ERROR] Cannot set a null alert mode");
    return;
  }

  mode->modeSetup();
  this->alertMode = mode;
}

void Controller::printSwitchedMode(AbstractMode* mode) {
  Serial.print("[INFO] Switched to mode '");
  Serial.print(mode->getTitle());
  Serial.print("' by '");
  Serial.print(mode->getAuthor());
  Serial.println("'");
}

AbstractMode* Controller::findMode(const String& title) {
  for (int i = 0; i < this->modes.size(); i++) {
    if (this->modes.get(i)->getTitle() == title) {
      return this->modes.get(i);
    }
  }

  return nullptr;
}

int16_t Controller::findModeIndex(AbstractMode* mode) {
  for (int i = 0; i < this->modes.size(); i++) {
    if (this->modes.get(i) == mode) {
      return i;
    }
  }

  return -1;
}

bool Controller::transitionTo(AbstractMode* mode) {
  if (mode == nullptr) {
    Serial.println("[ERROR] Cannot transition to a null mode");
    return false;
  }

  if (this->currentMode == mode) {
    return false;
  }

  AbstractMode* outgoingMode = this->currentMode;
  if (outgoingMode != nullptr) {
    outgoingMode->last();
  }

  this->currentMode = mode;
  this->previousMode = outgoingMode;

  int16_t modeIndex = this->findModeIndex(mode);
  if (modeIndex >= 0) {
    this->currentModeIndex = modeIndex;
  }

  this->printSwitchedMode(mode);
  mode->first();

  return true;
}

void Controller::synchronizeDistance() {
  if (this->distanceService->consumeLevelChange()) {
    this->levelUpdatePending = true;
  }

  if (!this->levelUpdatePending || millis() - this->lastLevelUpdate < LEVEL_UPDATE_INTERVAL) {
    return;
  }

  result_t result = this->distanceService->getResult();
  this->communicationService->sendDistanceUpdate(result.distance, result.level);

  this->lastLevelUpdate = millis();
  this->levelUpdatePending = false;
}

void Controller::nextMode() {
  if (this->modes.size() == 0) {
    Serial.println("[ERROR] No modes available");
    return;
  }

  uint8_t nextModeIndex = (this->currentModeIndex + 1) % this->modes.size();
  AbstractMode* mode = this->modes.get(nextModeIndex);

  if (mode == nullptr) {
    Serial.println("[ERROR] nextMode - Mode is null");
    return;
  }

  if (this->transitionTo(mode)) {
    this->event();
  }
}

void Controller::setMode(String title) {
  AbstractMode* targetMode = this->findMode(title);

  if (targetMode == nullptr) {
    Serial.print("[ERROR] Mode '");
    Serial.print(title);
    Serial.println("' not found");
    return;
  }

  this->transitionTo(targetMode);
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
  this->currentMode->customClick();

  this->event();
}

String Controller::getCurrentModeTitle() {
  return this->currentMode == nullptr ? "" : this->currentMode->getTitle();
}

uint8_t Controller::getCurrentOption() {
  return this->currentMode == nullptr ? 0 : this->currentMode->getCurrentOption();
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

  this->communicationService->onNewConnection(std::bind(&Controller::newConnectionCallback, this, std::placeholders::_1));
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

  this->synchronizeDistance();

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

  if (this->distanceService->hasWipeDetected()) {
    this->event();
    this->communicationService->sendWipe(this->distanceService->getNumberOfWipes());
  }
}

// alert functions
void Controller::enableAlert(uint8_t flashes, CRGB color) {
  if (this->alertMode == nullptr) {
    Serial.println("[ERROR] Alert mode is null");
    return;
  }

  if (this->currentMode == this->alertMode) {
    return;
  }

  this->alertMode->setColor(color);
  this->alertMode->setFlashes(flashes);
  this->transitionTo(this->alertMode);
}

void Controller::enableAlert(uint8_t flashes) {
  this->enableAlert(flashes, CRGB(255, 128, 20));
}

void Controller::disableAlert() {
  if (!this->alertEnabled()) {
    return;
  }

  AbstractMode* targetMode = this->previousMode;
  if (targetMode == nullptr || targetMode == this->alertMode) {
    if (this->modes.size() == 0) {
      Serial.println("[ERROR] No mode available after alert");
      return;
    }

    targetMode = this->modes.get(0);
  }

  this->transitionTo(targetMode);
}

bool Controller::alertEnabled() {
  return this->currentMode == this->alertMode;
}

// communication functions
void Controller::newConnectionCallback(uint32_t nodeId) {
  // Exactly one side publishes state, preventing symmetric discovery echoes.
  if (this->communicationService->getNodeId() < nodeId) {
    this->event();
  }

  uint64_t now = millis();
  if (!this->connectionAlertShown || now - this->lastConnectionAlert >= CONNECTION_ALERT_COOLDOWN) {
    this->connectionAlertShown = true;
    this->lastConnectionAlert = now;
    this->enableAlert(4, CRGB(0, 255, 0));
  }
}

void Controller::newMessageCallback(uint32_t from, JsonDocument message, MessageType type) {
  if (type == MessageType::EVENT) {
    if (!message["eventKind"].is<uint8_t>() || !message["mode"].is<JsonObject>() ||
        !message["mode"]["title"].is<String>() ||
        !message["mode"]["version"].is<String>() ||
        !message["payload"].is<JsonObject>()) {
      Serial.println("[ERROR] Invalid message event format, ignoring message");
      return;
    }

    uint8_t rawEventKind = message["eventKind"].as<uint8_t>();
    if (rawEventKind >= static_cast<uint8_t>(ModeEventKind::MAX)) {
      Serial.println("[ERROR] Invalid mode event kind, ignoring message");
      return;
    }

    String title = message["mode"]["title"].as<String>();
    String version = message["mode"]["version"].as<String>();
    AbstractMode* targetMode = this->findMode(title);

    if (targetMode == nullptr) {
      Serial.println("[ERROR] Event targets an unknown mode, ignoring message");
      return;
    }

    if (targetMode->getVersion() != version) {
      Serial.println("[ERROR] Event mode version mismatch, ignoring message");
      return;
    }

    if (this->currentMode != targetMode) {
      this->setMode(title);
    }

    ModeEventKind eventKind = static_cast<ModeEventKind>(rawEventKind);
    if (eventKind == ModeEventKind::STATE) {
      JsonDocument state;
      state["title"] = title;
      state["version"] = version;
      state["registry"] = message["payload"];
      this->currentMode->deserialize(state);
    } else {
      if (!message["command"].is<String>()) {
        Serial.println("[ERROR] Mode command is missing a command name");
        return;
      }

      JsonDocument payload;
      payload.set(message["payload"]);

      if (!this->currentMode->handleRemoteCommand(message["command"].as<String>(), payload)) {
        Serial.println("[ERROR] Mode command was not handled");
      }
    }
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

    // if the new GlowNode is younger, it will send the current state
    if (message["timestamp"].as<uint64_t>() < millis()) {
      this->event();
    }
  } else if (type == MessageType::WIPE) {
    // the WIPE message will be triggered if a wipe is detected

    // check if the wipe has the correct format
    if (!message["numberOfWipes"].is<uint16_t>()) {
      Serial.println("[ERROR] Invalid message wipe format, ignoring message");
      return;
    }

    // set the number of wipes
    this->distanceService->setNumberOfWipes(message["numberOfWipes"].as<uint16_t>());
  } else if (type == MessageType::LEVEL) {
    // Live dimming from a remote node is separate from local sensor state.

    // Check format
    if (!message["distance"].is<uint16_t>() || !message["level"].is<uint16_t>() ||
        !message["active"].is<bool>() || !message["active"].as<bool>()) {
      Serial.println("[ERROR] Invalid message level format, ignoring message");
      return;
    }

    uint16_t distance = message["distance"].as<uint16_t>();
    uint16_t level = message["level"].as<uint16_t>();

    // A locally operated sensor owns the output until the hand is removed.
    if (this->distanceService->isObjectPresent()) {
      return;
    }

    // Apply immediately to LEDs without feeding the value back into the sensor.
    if (this->currentMode != nullptr) {
      this->currentMode->applyRemoteUpdate(distance, level);
    }
  } else {
    Serial.println("[ERROR] Invalid message type, ignoring message");
  }
}

void Controller::event() {
  this->communicationService->sendStateEvent(this->currentMode->serialize());
}
