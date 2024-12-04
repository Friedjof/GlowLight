#include "MiniGame.h"

MiniGame::MiniGame(LightService *lightService, DistanceService *distanceService) : AbstractMode(lightService, distanceService) {
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

  this->lightService->fill(CRGB::Black);
}

void MiniGame::customLoop() {
  if (this->running) {
    if (this->counter % this->speed == 0) {
      this->position = (this->position + 1) % LED_NUM_LEDS;
      
      for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
        if (i == this->position) {
          continue;
        }

        if (i == this->goalIndex) {
          this->lightService->setLed(i, CRGB::Green);
        } else {
          this->lightService->setLed(i, CRGB::Black);
        }
      }

      if (this->position == this->goalIndex) {
        this->lightService->setLed(this->position, CRGB::Gold);
        this->lightService->show();
      } else {
        this->lightService->setLed(this->position, CRGB::White);
        this->lightService->show();
      }
    }

    this->counter++;

    this->newSpeed();
  }
}

void MiniGame::last() {
  Serial.println("[INFO] MiniGame last");
}

void MiniGame::customClick() {
  Serial.println("[INFO] MiniGame customClick");

  if (!this->running) {
    return;
  }

  if (this->goalIndex != this->position) {
    this->lightService->setLed(this->goalIndex, CRGB::Black);
  } else {
    this->lightService->setLed(this->goalIndex, CRGB::White);
  }

  this->goalIndex = (this->goalIndex + 1) % LED_NUM_LEDS;

  if (this->goalIndex != this->position) {
    this->lightService->setLed(this->goalIndex, CRGB::Green);
  } else {
    this->lightService->setLed(this->goalIndex, CRGB::Gold);
  }

  this->lightService->show();
}

bool MiniGame::newSpeed() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  level = map(level, 0, DISTANCE_LEVELS, MINIGAME_SPEED_MIN, MINIGAME_SPEED_MAX);

  if (level == this->speed) {
    return false;
  }

  this->speed = level;

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
    Serial.println("[INFO] You won!");
  } else {
    Serial.println("[INFO] Just try again!");
  }
}

void MiniGame::win() {
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->lightService->setLed(i, random(0, 255), random(0, 255), random(0, 255));
  }

  this->lightService->show();
}

void MiniGame::lose() {
  Serial.println("[INFO] MiniGame lose");

  this->lightService->fill(CRGB::Red);
}
