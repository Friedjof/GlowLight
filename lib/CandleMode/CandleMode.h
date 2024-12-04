#ifndef CANCLEMODE_H
#define CANCLEMODE_H

#include <Arduino.h>
#include <ArrayList.h>

#include "AbstractMode.h"

class CandleMode : public AbstractMode {
  public:
    CandleMode(LightService* lightService, DistanceService* distanceService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

  private:
    uint16_t speed = CANDLE_SPEED_DEFAULT;

    ArrayList<CRGB> colors;

    bool newSpeed();
};

#endif