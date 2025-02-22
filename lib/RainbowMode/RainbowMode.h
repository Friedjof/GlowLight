#ifndef RAINBOWMODE_H
#define RAINBOWMODE_H

#include <Arduino.h>

#include "AbstractMode.h"

class RainbowMode : public AbstractMode {
  public:
    RainbowMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newSaturation();
    bool newSpeed();

  private:
    uint64_t counter = 0;

    uint16_t index = 0;
};

#endif