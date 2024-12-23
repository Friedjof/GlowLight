#ifndef BEACONMODE_H
#define BEACONMODE_H

#include <Arduino.h>

#include "AbstractMode.h"

#include "GlowConfig.h"


class BeaconMode : public AbstractMode {
  public:
    BeaconMode(LightService* lightService, DistanceService* distanceService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();
  
  private:
    uint64_t counter = 0;
    uint16_t position = 0;

    uint8_t hueOne = 0;
    uint8_t hueTwo = 128;

    uint8_t speed = BEACON_SPEED_DEFAULT;

    bool smoothTransition = true;

    bool newSpeed();
    bool newHueOne();
    bool newHueTwo();

    uint16_t distance2hue(uint16_t distance, uint16_t currentHue);
    void setHue(uint16_t index, uint8_t hue);
};

#endif