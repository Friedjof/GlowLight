#ifndef COMMUNICATIONSERVICE_H
#define COMMUNICATIONSERVICE_H

#include <Arduino.h>
#include <ArrayList.h>
#include <ArduinoJson.h>
#include <painlessMesh.h>

#include "GlowConfig.h"

struct GlowNode {
  uint32_t id;
  uint32_t lastSeen;
};

class CommunicationService {
  private:
    painlessMesh* mesh = new painlessMesh();
    Scheduler* scheduler = nullptr;
    std::function<void()> alertCallback = nullptr;

    ArrayList<GlowNode> nodes;

    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionsCallback();
    void nodeTimeAdjustedCallback(int32_t offset);

  public:
    CommunicationService(Scheduler* scheduler);
    ~CommunicationService();

    void setup();
    void loop();

    void send(String message, GlowNode node);
    void broadcast(String message);

    ArrayList<GlowNode> getNodes();

    bool setNewConnectionCallback(std::function<void()> callback);
};

#endif