#ifndef COMMUNICATIONSERVICE_H
#define COMMUNICATIONSERVICE_H

#include <Arduino.h>
#include <time.h>

#include <ArrayList.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFi.h>

#include "GlowConfig.h"

struct GlowNode {
  uint32_t id;
  uint64_t lastSeen;

  bool operator==(const GlowNode& other) const {
    return this->id == other.id;
  }
};

enum MessageType {
  EVENT = 0,
  SYNC = 1,
  HEARTBEAT = 2,
  MAX
};

class CommunicationService {
  private:
    painlessMesh* mesh = new painlessMesh();
    Scheduler* scheduler = nullptr;

    std::function<void()> alertCallback = nullptr;
    std::function<void(uint32_t, JsonDocument, MessageType)> receivedControllerCallback = nullptr;

    ArrayList<GlowNode> nodes;

    // the CommunicationService will send periodic heartbeats to the other nodes to let them know it's still alive
    uint64_t last_hartbeat = 0;

    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionsCallback();
    void nodeTimeAdjustedCallback(int32_t offset);

    void addNode(uint32_t id);
    uint16_t getNode(uint32_t id, GlowNode* node);
    uint32_t seenNode(uint32_t id);
    void removeNode(uint32_t id);
    bool updateNode(uint32_t id);
    void removeOldNodes();
    bool nodeExists(uint32_t id);

    // time
    uint64_t getTimestamp();
    void setTimestamp(uint64_t timestamp);

    void send(String message, GlowNode node);
    void broadcast(String message);

  public:
    CommunicationService(Scheduler* scheduler);
    ~CommunicationService();

    void setup();
    void loop();

    ArrayList<GlowNode> getNodes();

    // communication
    void sendEvent(JsonDocument event);
    void sendSync(uint64_t timestamp);

    bool onNewConnection(std::function<void()> callback);
    bool onReceived(std::function<void(uint32_t, JsonDocument, MessageType)> callback);
};

#endif