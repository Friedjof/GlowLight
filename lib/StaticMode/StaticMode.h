#ifndef STATICMODE_H
#define STATICMODE_H

#include <Arduino.h>
#include <ArrayList.h>

#include "AbstractMode.h"


class StaticMode : public AbstractMode {
  public:
    StaticMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

  private:
    bool fixed = false;
};

#endif