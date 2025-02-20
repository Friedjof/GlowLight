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

    void printSwitchedMode(AbstractMode* mode);

    void event();

    void newConnectionCallback();
    void newMessageCallback(uint32_t from, JsonDocument doc, MessageType type);

  public:
    Controller(DistanceService* distanceService, CommunicationService* communicationService);

    void setAlertMode(Alert* mode);

    void addMode(AbstractMode* mode);
    void nextMode();
    void setMode(String title);

    void nextOption();
    void setOption(uint8_t option);
    void customClick();

    void setup();
    void loop();
};

#endif