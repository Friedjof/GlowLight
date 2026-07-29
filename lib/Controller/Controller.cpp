#include "Controller.h"

Controller::Controller(DistanceService* distanceService, CommunicationService* communicationService) {
  this->distanceService = distanceService;
  this->communicationService = communicationService;
  this->updatePublishingGate();
}

void Controller::configureSyncDefaults(bool follow, bool publish) {
  this->syncFollowDefault = follow;
  this->syncPublishDefault = publish;
  this->syncFollow = follow;
  this->syncPublish = publish;
  this->syncStatus = SyncStatus::Unavailable;
  this->updatePublishingGate();
}

void Controller::configureRuntimeFeatures(bool captivePortal, bool ota) {
  this->captivePortalSupported = captivePortal;
  this->otaEnabled = ota;
}

bool Controller::canPublish() const {
  return this->syncPublish && this->communicationService->isAvailable() &&
         this->syncStatus != SyncStatus::Joining;
}

void Controller::updatePublishingGate() {
  if (this->communicationService->setApplicationPublishing(this->canPublish())) {
    this->markLocalChange();
  }
}

const char* Controller::syncStatusName() const {
  switch (this->syncStatus) {
    case SyncStatus::Unavailable:
      return "unavailable";
    case SyncStatus::Detached:
      return "detached";
    case SyncStatus::Joining:
      return "joining";
    case SyncStatus::Synchronized:
      return "synchronized";
  }
  return "unavailable";
}

void Controller::addSyncState(JsonObject target) const {
  SyncVersion version = this->communicationService->syncVersion();
  target["follow"] = this->syncFollow;
  target["publish"] = this->syncPublish;
  target["status"] = this->syncStatusName();
  target["localDirty"] = this->localDirty;
  target["transportAvailable"] = this->communicationService->isAvailable();
  target["revision"] = version.revision;
  target["origin"] = version.origin;
}

bool Controller::syncVersionIsNewer(const SyncVersion& candidate) const {
  SyncVersion current = this->communicationService->syncVersion();
  return candidate.revision > current.revision ||
         (candidate.revision == current.revision && candidate.origin > current.origin);
}

void Controller::markLocalChange() {
  this->localDirty = true;
}

void Controller::beginRejoin() {
  if (!this->syncFollow) {
    this->syncStatus = this->communicationService->isAvailable()
                           ? SyncStatus::Detached
                           : SyncStatus::Unavailable;
    this->updatePublishingGate();
    return;
  }
  if (!this->communicationService->isAvailable()) {
    this->syncStatus = SyncStatus::Unavailable;
    this->updatePublishingGate();
    return;
  }

  this->syncStatus = SyncStatus::Joining;
  this->joinStartedAt = millis();
  this->lastSyncRequestAt = millis();
  this->updatePublishingGate();
  this->communicationService->sendSyncRequest();
}

void Controller::requestResync() {
  if (this->syncFollow) this->beginRejoin();
}

bool Controller::publishCurrentState() {
  AbstractMode* mode = this->synchronizedMode();
  if (mode == nullptr || !this->communicationService->sendStateEvent(mode->serialize())) {
    this->markLocalChange();
    return false;
  }
  this->localDirty = false;
  return true;
}

void Controller::maintainSync() {
  bool available = this->communicationService->isAvailable();
  if (!available) {
    if (this->syncStatus != SyncStatus::Unavailable) {
      this->syncStatus = SyncStatus::Unavailable;
      this->updatePublishingGate();
    }
    return;
  }

  if (this->syncStatus == SyncStatus::Unavailable) {
    if (this->syncFollow) this->beginRejoin();
    else {
      this->syncStatus = SyncStatus::Detached;
      this->updatePublishingGate();
    }
    return;
  }

  uint32_t now = millis();
  if (this->syncStatus == SyncStatus::Joining) {
    if (now - this->joinStartedAt >= JOIN_TIMEOUT_MS &&
        this->communicationService->getNodes().size() == 0) {
      this->syncStatus = SyncStatus::Synchronized;
      this->updatePublishingGate();
      if (this->syncPublish) this->publishCurrentState();
    } else if (now - this->lastSyncRequestAt >= JOIN_TIMEOUT_MS) {
      this->lastSyncRequestAt = now;
      this->communicationService->sendSyncRequest();
    }
    return;
  }

  if (this->syncFollow && now - this->lastSyncRequestAt >= SYNC_REQUEST_INTERVAL_MS) {
    this->lastSyncRequestAt = now;
    this->communicationService->sendSyncRequest();
  }
}

// mode functions
void Controller::addMode(AbstractMode* mode) {
  if (mode == nullptr) {
    Serial.println("[ERROR] Cannot add a null mode");
    return;
  }

  if (mode->getId().isEmpty() || this->findModeById(mode->getId()) != nullptr) {
    Serial.println("[ERROR] Mode ID is empty or already registered");
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

AbstractMode* Controller::findModeById(const String& id) {
  for (int i = 0; i < this->modes.size(); i++) {
    if (this->modes.get(i)->getId() == id) return this->modes.get(i);
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

bool Controller::transitionTo(AbstractMode* mode, bool stateRestored) {
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
  mode->first(stateRestored);

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
  if (this->canPublish()) {
    this->communicationService->sendDistanceUpdate(result.distance, result.level);
  } else {
    this->markLocalChange();
  }

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
  AbstractMode* mode = this->synchronizedMode();
  return mode == nullptr ? "" : mode->getTitle();
}

String Controller::getCurrentModeId() {
  AbstractMode* mode = this->synchronizedMode();
  return mode == nullptr ? "" : mode->getId();
}

uint8_t Controller::getCurrentOption() {
  AbstractMode* mode = this->synchronizedMode();
  return mode == nullptr ? 0 : mode->getCurrentOption();
}

JsonDocument Controller::capabilities() {
  JsonDocument document;
  document["schema"] = "glow.capabilities";
  document["schemaVersion"] = 1;
  document["controlApi"] = "glow.control/1";
  document["features"]["infrastructureWifi"] = true;
  document["features"]["groupControl"] =
      this->communicationService->isConfigured();
  document["features"]["syncPolicy"]["supported"] =
      this->communicationService->isConfigured();
  document["features"]["syncPolicy"]["controls"][0] = "follow";
  document["features"]["syncPolicy"]["controls"][1] = "publish";
  document["features"]["syncPolicy"]["runtimeMutable"] = true;
  document["features"]["syncPolicy"]["defaults"]["follow"] =
      this->syncFollowDefault;
  document["features"]["syncPolicy"]["defaults"]["publish"] =
      this->syncPublishDefault;
  document["features"]["captivePortal"] = this->captivePortalSupported;
  document["features"]["ota"] = this->otaEnabled;

  JsonArray modes = document["modes"].to<JsonArray>();
  for (size_t index = 0; index < this->modes.size(); ++index) {
    modes.add(this->modes.get(index)->capabilities());
  }
  return document;
}

JsonDocument Controller::state() {
  JsonDocument document;
  document["schema"] = "glow.state";
  document["schemaVersion"] = 1;
  this->addSyncState(document["sync"].to<JsonObject>());
  AbstractMode* mode = this->synchronizedMode();
  if (mode != nullptr) document["mode"] = mode->serialize();
  return document;
}

JsonDocument Controller::executeControl(const JsonDocument& request) {
  JsonDocument response;
  response["api"] = "glow.control/1";
  if (!request["requestId"].isNull()) response["requestId"] = request["requestId"];

  auto fail = [&response](const char* code, const char* message) {
    response["ok"] = false;
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    return response;
  };

  if (!request["api"].is<String>() ||
      request["api"].as<String>() != "glow.control/1") {
    return fail("INVALID_API", "Expected api glow.control/1");
  }
  if (!request["operation"].is<String>()) {
    return fail("INVALID_OPERATION", "Operation is required");
  }

  String operation = request["operation"].as<String>();
  if (operation == "capabilities.get") {
    response["ok"] = true;
    response["capabilities"] = this->capabilities();
    return response;
  }
  if (operation == "state.get") {
    response["ok"] = true;
    response["state"] = this->state();
    return response;
  }

  if (operation == "sync.configure") {
    String syncScope = request["scope"] | "local";
    if (syncScope != "local") {
      return fail("INVALID_SCOPE", "Sync policy can only be changed locally");
    }
    if (!request["sync"]["follow"].is<bool>() ||
        !request["sync"]["publish"].is<bool>()) {
      return fail("INVALID_SYNC_POLICY", "Follow and publish booleans are required");
    }

    bool wasFollowing = this->syncFollow;
    this->syncFollow = request["sync"]["follow"].as<bool>();
    this->syncPublish = request["sync"]["publish"].as<bool>();
    if (this->syncFollow && (!wasFollowing ||
                             this->syncStatus == SyncStatus::Unavailable)) {
      this->beginRejoin();
    } else if (!this->syncFollow) {
      this->syncStatus = this->communicationService->isAvailable()
                             ? SyncStatus::Detached
                             : SyncStatus::Unavailable;
      this->updatePublishingGate();
    } else {
      this->updatePublishingGate();
    }

    response["ok"] = true;
    this->addSyncState(response["sync"].to<JsonObject>());
    return response;
  }

  String scope = request["scope"] | "local";
  if (scope != "local" && scope != "group") {
    return fail("INVALID_SCOPE", "Scope must be local or group");
  }
  bool group = scope == "group";
  if (group && !this->communicationService->isAvailable()) {
    return fail("GROUP_UNAVAILABLE", "Group communication is not available");
  }
  if (group && (!this->syncPublish || this->syncStatus == SyncStatus::Joining)) {
    return fail("SYNC_PUBLISH_DISABLED", "Publishing to the group is disabled");
  }

  if (!request["target"]["mode"].is<String>()) {
    return fail("INVALID_TARGET", "A target mode ID is required");
  }
  AbstractMode* target = this->findModeById(request["target"]["mode"].as<String>());
  if (target == nullptr) return fail("UNKNOWN_MODE", "Target mode is not available");

  if (operation == "mode.select") {
    this->transitionTo(target);
    // Group selection also acts as convergence: publish state even when this
    // lamp already had the requested mode selected.
    if (group) {
      if (!this->event()) {
        return fail("GROUP_PUBLISH_FAILED", "Mode state could not be published");
      }
    } else {
      this->markLocalChange();
    }
  } else {
    if (target != this->synchronizedMode()) {
      return fail("MODE_NOT_ACTIVE", "Target mode is not active");
    }

    if (operation == "mode.option.set") {
      if (!request["option"].is<uint8_t>() ||
          request["option"].as<uint8_t>() >= target->getNumberOfOptions()) {
        return fail("INVALID_OPTION", "Option index is out of range");
      }
      target->setOption(request["option"].as<uint8_t>());
      if (group) {
        if (!this->event()) {
          return fail("GROUP_PUBLISH_FAILED", "Mode state could not be published");
        }
      } else {
        this->markLocalChange();
      }
    } else if (operation == "mode.setting.set") {
      if (!request["setting"].is<String>() || request["value"].isNull() ||
          !target->setSetting(request["setting"].as<String>(), request["value"])) {
        return fail("INVALID_SETTING", "Setting is unknown, read-only, or invalid");
      }
      if (group) {
        if (!this->event()) {
          return fail("GROUP_PUBLISH_FAILED", "Mode state could not be published");
        }
      } else {
        this->markLocalChange();
      }
    } else if (operation == "mode.command") {
      if (!request["command"].is<String>() ||
          !request["arguments"].is<JsonObjectConst>()) {
        return fail("INVALID_COMMAND", "Command and argument object are required");
      }
      JsonDocument arguments;
      arguments.set(request["arguments"]);
      String command = request["command"].as<String>();
      if (group && !target->commandSupportsGroup(command)) {
        return fail("COMMAND_NOT_GROUP_CAPABLE", "Command cannot target a group");
      }
      if (group && this->localDirty) {
        return fail("LOCAL_STATE_DIVERGED",
                    "Publish the local mode state before sending a group command");
      }
      if (!target->executeCommand(command, arguments)) {
        return fail("COMMAND_REJECTED", "Mode rejected the command or arguments");
      }
      if (group) {
        if (!this->communicationService->sendModeCommand(
                target->getId(), target->getTitle(), target->getVersion(),
                target->getStateSchemaVersion(), command, arguments)) {
          this->markLocalChange();
          return fail("GROUP_PUBLISH_FAILED", "Group command could not be published");
        }
        this->localDirty = false;
      } else this->markLocalChange();
    } else {
      return fail("INVALID_OPERATION", "Operation is not supported");
    }
  }

  response["ok"] = true;
  return response;
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

  this->beginRejoin();

  Serial.println("[INFO] Controller initialized");

  this->enableAlert(5);
}

void Controller::loop() {
  this->maintainSync();

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
    if (this->canPublish()) {
      this->communicationService->sendWipe(this->distanceService->getNumberOfWipes());
    }
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
  if (this->syncFollow) this->beginRejoin();

  uint64_t now = millis();
  if (!this->connectionAlertShown || now - this->lastConnectionAlert >= CONNECTION_ALERT_COOLDOWN) {
    this->connectionAlertShown = true;
    this->lastConnectionAlert = now;
    this->enableAlert(4, CRGB(0, 255, 0));
  }
}

void Controller::handleSyncMessage(const JsonDocument& message) {
  if (!message["kind"].is<String>() ||
      message["kind"].as<String>() != "state.request" ||
      !message["requester"].is<uint32_t>()) {
    Serial.println("[ERROR] Invalid sync message, ignoring message");
    return;
  }

  uint32_t requester = message["requester"].as<uint32_t>();
  uint32_t self = this->communicationService->getNodeId();
  if (requester == self || this->localDirty) return;

  // A snapshot spans several fragments, so every peer answering at once would
  // flood a sixteen slot transmit queue. The lowest node id speaks for the
  // group, the same tie-break newConnectionCallback already uses.
  const ArrayList<GlowNode>& nodes = this->communicationService->getNodes();
  for (size_t i = 0; i < nodes.size(); ++i) {
    uint32_t peer = nodes.get(i).id;
    if (peer != requester && peer < self) return;
  }

  AbstractMode* mode = this->synchronizedMode();
  if (mode != nullptr) {
    this->communicationService->sendStateSnapshot(mode->serialize(), requester);
  }
}

void Controller::newMessageCallback(uint32_t from, JsonDocument message, MessageType type) {
  if (type == MessageType::EVENT) {
    if (!message["eventKind"].is<uint8_t>() || !message["mode"].is<JsonObject>() ||
        !message["mode"]["id"].is<String>() ||
        !message["mode"]["title"].is<String>() ||
        !message["mode"]["version"].is<String>() ||
        !message["mode"]["schemaVersion"].is<uint16_t>() ||
        !message["sync"]["revision"].is<uint64_t>() ||
        !message["sync"]["origin"].is<uint32_t>() ||
        !message["payload"].is<JsonObject>()) {
      Serial.println("[ERROR] Invalid message event format, ignoring message");
      return;
    }

    if (!this->syncFollow) return;

    SyncVersion candidate(message["sync"]["revision"].as<uint64_t>(),
                          message["sync"]["origin"].as<uint32_t>());
    if (candidate.revision == 0 || candidate.origin == 0) return;
    uint8_t rawEventKind = message["eventKind"].as<uint8_t>();
    if (rawEventKind >= static_cast<uint8_t>(ModeEventKind::MAX)) {
      Serial.println("[ERROR] Invalid mode event kind, ignoring message");
      return;
    }
    ModeEventKind eventKind = static_cast<ModeEventKind>(rawEventKind);
    if (this->syncStatus == SyncStatus::Joining &&
        eventKind != ModeEventKind::STATE) return;
    bool replyToUs = message["sync"]["replyTo"].is<uint32_t>() &&
                      message["sync"]["replyTo"].as<uint32_t>() ==
                          this->communicationService->getNodeId();
    String id = message["mode"]["id"].as<String>();
    String title = message["mode"]["title"].as<String>();
    String version = message["mode"]["version"].as<String>();
    uint16_t schemaVersion = message["mode"]["schemaVersion"].as<uint16_t>();
    AbstractMode* targetMode = this->findModeById(id);

    if (targetMode == nullptr) {
      Serial.println("[ERROR] Event targets an unknown mode, ignoring message");
      return;
    }

    if (targetMode->getStateSchemaVersion() != schemaVersion) {
      Serial.println("[ERROR] Event state schema version mismatch, ignoring message");
      return;
    }

    SyncVersion current = this->communicationService->syncVersion();
    bool sameVersion = candidate.revision == current.revision &&
                       candidate.origin == current.origin;
    bool replySnapshot = replyToUs && eventKind == ModeEventKind::STATE;
    bool shouldApply = this->syncVersionIsNewer(candidate) ||
                       (replySnapshot && sameVersion);
    if (!shouldApply) {
      bool currentIsNewer = current.revision > candidate.revision ||
                            (current.revision == candidate.revision &&
                             current.origin > candidate.origin);
      if (replySnapshot && currentIsNewer &&
          this->syncStatus == SyncStatus::Joining && !this->localDirty) {
        AbstractMode* mode = this->synchronizedMode();
        if (mode != nullptr &&
            this->communicationService->sendStateSnapshot(mode->serialize(), from)) {
          this->syncStatus = SyncStatus::Synchronized;
          this->updatePublishingGate();
        }
      }
      return;
    }

    if (eventKind == ModeEventKind::STATE) {
      JsonDocument state;
      state["id"] = id;
      state["title"] = title;
      state["version"] = version;
      state["schemaVersion"] = schemaVersion;
      state["registry"] = message["payload"];
      if (!targetMode->deserialize(state)) {
        Serial.println("[ERROR] Mode state was rejected");
        return;
      }
      if (this->currentMode != targetMode) this->transitionTo(targetMode, true);
    } else {
      if (!message["command"].is<String>()) {
        Serial.println("[ERROR] Mode command is missing a command name");
        return;
      }

      JsonDocument payload;
      payload.set(message["payload"]);

      if (!targetMode->acceptsCommand(message["command"].as<String>(), payload)) {
        Serial.println("[ERROR] Mode command or arguments were rejected");
        return;
      }
      AbstractMode* previousMode = this->currentMode;
      bool transitioned = previousMode != targetMode && this->transitionTo(targetMode);
      if (!targetMode->executeCommand(message["command"].as<String>(), payload)) {
        if (transitioned && previousMode != nullptr) {
          this->transitionTo(previousMode, true);
        }
        Serial.println("[ERROR] Mode command was not handled");
        return;
      }
    }
    this->communicationService->acceptSyncVersion(candidate);
    this->localDirty = false;
    if (this->syncStatus == SyncStatus::Joining) {
      this->syncStatus = SyncStatus::Synchronized;
      this->updatePublishingGate();
    }
  } else if (type == MessageType::SYNC) {
    this->handleSyncMessage(message);
  } else if (type == MessageType::WIPE) {
    if (!this->syncFollow) return;
    // the WIPE message will be triggered if a wipe is detected

    // check if the wipe has the correct format
    if (!message["numberOfWipes"].is<uint16_t>()) {
      Serial.println("[ERROR] Invalid message wipe format, ignoring message");
      return;
    }

    // set the number of wipes
    this->distanceService->setNumberOfWipes(message["numberOfWipes"].as<uint16_t>());
  } else if (type == MessageType::LEVEL) {
    if (!this->syncFollow) return;
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

AbstractMode* Controller::synchronizedMode() {
  // The alert is a local visual overlay, not a mode peers can switch to: it is
  // never in the mode list, so broadcasting it makes every peer reject the
  // event. While it runs, the logical mode is the one the alert returns to.
  if (this->currentMode != this->alertMode) {
    return this->currentMode;
  }

  if (this->previousMode != nullptr && this->previousMode != this->alertMode) {
    return this->previousMode;
  }

  return this->modes.size() > 0 ? this->modes.get(0) : nullptr;
}

bool Controller::event() {
  AbstractMode* mode = this->synchronizedMode();
  if (mode == nullptr) {
    return false;
  }

  if (!this->canPublish()) {
    this->markLocalChange();
    return false;
  }
  if (!this->communicationService->sendStateEvent(mode->serialize())) {
    this->markLocalChange();
    return false;
  }
  this->localDirty = false;
  return true;
}
