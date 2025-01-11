#ifndef COMMUNICATIONSERVICE_H
#define COMMUNICATIONSERVICE_H

#include <Arduino.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <painlessMesh.h>

#include "GlowConfig.h"

class CommunicationService {
  public:
    CommunicationService();
    ~CommunicationService();

    void setup();
    void loop();
    void sendMessage(String message);
};

#endif