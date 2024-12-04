#ifndef MINIGAME_H
#define MINIGAME_H

#include <AbstractMode.h>

class MiniGame : public AbstractMode {
  public:
    MiniGame(LightService *lightService, DistanceService *distanceService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newSpeed();

    void run();
    void stop();

    void win();

    void updateLightSteps();

  private:
    uint16_t speed = MINIGAME_SPEED_DEFAULT;
    uint16_t position = 0;
    uint16_t goalIndex = 0;

    bool running = false;
    bool won = false;

    uint64_t counter = 0;
};

#endif
