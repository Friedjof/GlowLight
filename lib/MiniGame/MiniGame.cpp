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
  if (this-running) {
    if (this->counter % this->speed == 0) {
      Serial.print("[DEBUG] Position: ");
      Serial.println(this->position);

      if(this->position == this->goalIndex) {
        Serial.println("[DEBUG] goalIndex reached");
      }

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

      this->lightService->setLed(this->position, CRGB::White);
      this->lightService->show();
    }
  }

  /**
   * If you merge the two if statements, the game will not be stoppable anymore
   * For some reason the condition will be ignored. Is this a bug in the compiler?
   */
  if (this->running) {
    this->counter++;

    this->newSpeed();
  }
}

void MiniGame::last() {
  Serial.println("[INFO] MiniGame last");
}

void MiniGame::customClick() {
  Serial.println("[INFO] MiniGame customClick");

  this->lightService->setLed(this->goalIndex, CRGB::Black);
  this->lightService->show();

  this->goalIndex = (this->goalIndex + 1) % LED_NUM_LEDS;
}

bool MiniGame::newSpeed() {
  if (!this->distanceService->objectPresent()) {
    return false;
  }

  uint16_t level = this->getLevel();

  level = this->invExpNormalize(level, 0, DISTANCE_LEVELS, MINIGAME_SPEED_MAX, .5);

  if (level < MINIGAME_SPEED_MIN) {
    level = MINIGAME_SPEED_MIN;
  } else if (level > MINIGAME_SPEED_MAX) {
    level = MINIGAME_SPEED_MAX;
  }

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
