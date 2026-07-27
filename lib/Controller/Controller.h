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

    bool levelUpdatePending = false;
    uint64_t lastLevelUpdate = 0;
    uint64_t lastConnectionAlert = 0;
    bool connectionAlertShown = false;

    static constexpr uint32_t CONNECTION_ALERT_COOLDOWN = 30000;

    void enableAlert(uint8_t flashes, CRGB color);
    void enableAlert(uint8_t flashes);
    void disableAlert();
    bool alertEnabled();

    void printSwitchedMode(AbstractMode* mode);
    AbstractMode* findMode(const String& title);
    int16_t findModeIndex(AbstractMode* mode);
    bool transitionTo(AbstractMode* mode);
    void synchronizeDistance();

    void event();

    void newConnectionCallback(uint32_t nodeId);
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

    String getCurrentModeTitle();
    uint8_t getCurrentOption();

    void setup();
    void loop();
};

#endif
