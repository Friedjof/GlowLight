#ifndef COLORPICKERMODE_H
#define COLORPICKERMODE_H

#include <Arduino.h>

#include "AbstractMode.h"


class ColorPickerMode : public AbstractMode {
  public:
    ColorPickerMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newHue();
    bool newSaturation();

    uint16_t distance2hue(uint16_t distance);
};

#endif