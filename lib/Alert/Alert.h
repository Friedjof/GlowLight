#ifndef ALERT_H
#define ALERT_H

#include <Arduino.h>

#include "AbstractMode.h"


class Alert : public AbstractMode {
  public:
    Alert(LightService* lightService, DistanceService* distanceService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool isFlashing();

    bool setFlashes(uint8_t flashes);
    bool setColor(CRGB color);

  private:
    CRGB color = CRGB::GreenYellow;

    uint16_t index = 0;
    bool flashing = false;
    uint8_t flashes = ALERT_NUM_FLASHES;
};

#endif