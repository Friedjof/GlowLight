#include "Controller.h"


Controller::Controller(GlowRegistry* registry, LightService* lightService, DistanceService* distanceService, ButtonService* buttonService, LinkService* linkService) {
  this->registry = registry;
  this->lightService = lightService;
  this->distanceService = distanceService;
  this->buttonService = buttonService;
  this->linkService = linkService;
}

Controller::~Controller() {
}

void Controller::setup() {
  this->lightService->fill(CRGB::Green);
  this->lightService->show();

  String active = this->registry->getActive();
  this->registry->setActive("initial", false);

  Serial.println("[INFO] Controller setup complete");
}

void Controller::loop() {
  if (this->registry->getActive() == "initial") {
    this->initSequences();
  } else {
    this->distanceService->loop();
    this->buttonService->loop();
  }
  
  this->lightService->loop();
  this->linkService->loop();
}

void Controller::receive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  this->linkService->receive(info, data, len);
}

String Controller::command(JsonDocument doc) {
  if (!doc["command"].is<int>()) {
    Serial.println("[WARN] Controller: Received command without command type");
    return "";
  }

  Command command = static_cast<Command>(doc["command"].as<int>());
  if (command == Command::NAMESPACE) {
    if (!doc["namespace"].is<String>()) {
      return;
    }

    String nameSpace = doc["namespace"].as<String>();
    this->registry->setActive(nameSpace, false);
  } else if (command == Command::SYNC) {
    return this->registry->sync();
  } else if (command == Command::ACK) {
    if (!doc["callback"].is<int>()) {
      return "";
    }

    Command callback = static_cast<Command>(doc["callback"].as<int>());
    if (callback == Command::SYNC) {
      String _namespace = doc["namespace"].as<String>();
      JsonObject data = doc["data"].as<JsonObject>();
      this->registry->setActive(_namespace, false);

      this->registry->syncNamespace(_namespace, data);
    }
  } else {
    Serial.println("[WARN] Controller: Received unknown command");
  }

  return "";
}

void Controller::simpleClickHandler() {
  this->linkService->printPeers();
  Serial.println("[INFO] Simple click detected");
}

void Controller::doubleClickHandler() {
  this->linkService->printPeers();
  Serial.println("[INFO] Double click detected");
}

void Controller::longClickHandler() {
  this->linkService->printPeers();
  Serial.println("[INFO] Long click detected");
}

void Controller::initSequences() {
  uint64_t now = millis();
  if (now - this->lastSequence < 25) {
    return;
  }

  if (this->initSequenceCompleted) {
    this->lastSequence = now;
    this->registry->setActive("default");
    return;
  }

  if (!this->initSequenceInitialized) {
    this->lightService->setBrightness(LED_MIN_BRIGHTNESS);
    this->initSequenceInitialized = true;
    this->initSequenceDirection = 1;
    this->initSequenceCycles = 0;
  }

  const uint16_t configuredStep = LED_UPDATE_STEPS;
  const uint8_t step = configuredStep == 0 ? 1 : (configuredStep > 255 ? 255 : static_cast<uint8_t>(configuredStep));
  uint8_t brightness = this->lightService->getBrightness();

  if (this->initSequenceDirection > 0) {
    if (brightness >= LED_MAX_BRIGHTNESS) {
      this->initSequenceDirection = -1;
    } else {
      uint16_t target = brightness + step;
      if (target > LED_MAX_BRIGHTNESS) {
        target = LED_MAX_BRIGHTNESS;
      }
      this->lightService->setBrightness(static_cast<uint8_t>(target));
    }
  } else {
    if (brightness <= LED_MIN_BRIGHTNESS) {
      ++this->initSequenceCycles;
      if (this->initSequenceCycles >= 2) {
        this->lightService->setBrightness(LED_DEFAULT_BRIGHTNESS);
        this->initSequenceCompleted = true;
        Serial.println("Init Sequence Completed");
      } else {
        this->initSequenceDirection = 1;
      }
    } else {
      int16_t target = static_cast<int16_t>(brightness) - step;
      if (target < LED_MIN_BRIGHTNESS) {
        target = LED_MIN_BRIGHTNESS;
      }
      this->lightService->setBrightness(static_cast<uint8_t>(target));
    }
  }

  this->lastSequence = now;
}
