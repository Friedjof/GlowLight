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
    CRGB getColor();

  private:
    CRGB color = CRGB(255, 128, 20); // Warmer pink

    uint16_t index = 0;
    bool flashing = false;
    uint8_t flashes = ALERT_NUM_FLASHES;
};

#endif