#ifndef CANCLEMODE_H
#define CANCLEMODE_H

#include <Arduino.h>
#include <ArrayList.h>

#include "AbstractMode.h"

class CandleMode : public AbstractMode {
  public:
    CandleMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

  private:
    ArrayList<CRGB> colors;

    bool newSpeed();
};

#endif