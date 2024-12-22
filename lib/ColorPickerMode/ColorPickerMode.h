#ifndef COLORPICKERMODE_H
#define COLORPICKERMODE_H

#include <Arduino.h>

#include "AbstractMode.h"


class ColorPickerMode : public AbstractMode {
  public:
    ColorPickerMode(LightService* lightService, DistanceService* distanceService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newHue();
    bool newSaturation();

    uint16_t distance2hue(uint16_t distance);

  private:
    uint64_t hue = 0;
    uint8_t saturation = 255;
    bool fixed = false;
};

#endif