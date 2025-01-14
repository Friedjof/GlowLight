#include "MiniGame.h"

MiniGame::MiniGame(LightService *lightService, DistanceService *distanceService, CommunicationService *communicationService) : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "MiniGame";
  this->description = "With this game you can test your reaction time";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "MIT";
}

void MiniGame::setup() {
  Serial.println("[INFO] MiniGame setup");

  this->addOption("Run", std::function<void()>([this](){ this->run(); }), false, true);
  this->addOption("Stop", std::function<void()>([this](){ this->stop(); }), false, true);
}

void MiniGame::customFirst() {
  this->counter = 0;
  this->updateBrightness(64);

  this->lightService->setLightUpdateSteps(map(
    this->speed, MINIGAME_SPEED_MIN, MINIGAME_SPEED_MAX,
    LED_UPDATE_STEPS_MAX, LED_UPDATE_STEPS_MIN
  ));

  this->lightService->fill(CRGB::Black);
}

void MiniGame::customLoop() {
  if (!this->running && this->won) {
    this->win();
  }

  if (this->running) {
    if (this->counter % this->speed == 0) {
      this->position = (this->position + 1) % LED_NUM_LEDS;

      for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
        if (i == this->position) {
          continue;
        }

        this->lightService->updateLed(i, i == this->goalIndex ? CRGB::Green : CRGB::Black);
      }

      this->lightService->updateLed(
        this->position, this->position == this->goalIndex ? CRGB::Gold : CRGB::White);
    }
  }

  this->counter++;

  this->newSpeed();
}

void MiniGame::last() {
  Serial.println("[INFO] MiniGame last");
}

void MiniGame::customClick() {
  Serial.println("[INFO] MiniGame customClick");

  if (!this->running) {
    return;
  }

  this->lightService->setLed(
    this->goalIndex, this->goalIndex != this->position ? CRGB::Black : CRGB::White);

  this->goalIndex = (this->goalIndex + 1) % LED_NUM_LEDS;

  this->lightService->updateLed(
    this->goalIndex, this->goalIndex != this->position ? CRGB::Green : CRGB::Gold);
}

bool MiniGame::newSpeed() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  level = map(level, 0, DISTANCE_LEVELS, MINIGAME_SPEED_MIN, MINIGAME_SPEED_MAX);

  if (level == this->speed) {
    return false;
  }

  this->speed = level;

  this->updateLightSteps();

  return true;
}

void MiniGame::run() {
  Serial.println("[INFO] MiniGame run");

  this->running = true;
}

void MiniGame::stop() {
  Serial.println("[INFO] MiniGame stop");

  this->running = false;

  if (this->position == this->goalIndex) {
    this->won = true;

    this->win();
  } else {
    this->won = false;

    this->lightService->fill(CRGB::Red);
  }
}

void MiniGame::win() {
  if (this->counter % 4 != 0) {
    return;
  }

  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->lightService->setLed(i, random(0, 2) == 0 ? CRGB::Black : CRGB::Gold);
  }
}

void MiniGame::updateLightSteps() {
  this->lightService->setLightUpdateSteps(map(
    this->speed, MINIGAME_SPEED_MIN, MINIGAME_SPEED_MAX,
    LED_UPDATE_STEPS_MAX, LED_UPDATE_STEPS_MIN
  ));
}
