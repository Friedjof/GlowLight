#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>
#include <ArrayList.h>

#include "AbstractMode.h"
#include "Alert.h"

#include "DistanceService.h"
#include "CommunicationService.h"


class Controller {
  private:
    ArrayList<AbstractMode*> modes;
    Alert* alertMode = nullptr;

    uint8_t currentModeIndex = 0;
    AbstractMode* currentMode = nullptr;
    AbstractMode* previousMode = nullptr;

    DistanceService* distanceService;
    CommunicationService* communicationService;

    void enableAlert(uint8_t flashes, CRGB color);
    void enableAlert(uint8_t flashes);
    void disableAlert();
    bool alertEnabled();

    void newConnectionCallback();

  public:
    Controller(DistanceService* distanceService, CommunicationService* communicationService);

    void addMode(AbstractMode* mode);

    void setAlertMode(Alert* mode);

    void nextMode();
    void nextOption();
    void customClick();

    void setup();
    void loop();
};

#endif