#ifndef COMMUNICATIONSERVICE_H
#define COMMUNICATIONSERVICE_H

#include <Arduino.h>
#include <time.h>

#include <ArrayList.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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
  WIPE = 3,
  LEVEL = 4,
  MAX
};

enum class ModeEventKind : uint8_t {
  STATE = 0,
  COMMAND = 1,
  MAX
};

class CommunicationService {
  private:
    static constexpr uint16_t MESSAGE_HEADER_SIZE = 12;
    static constexpr uint16_t MAX_PAYLOAD_SIZE = ESP_NOW_MAX_DATA_LEN - MESSAGE_HEADER_SIZE;
    static constexpr uint8_t RECEIVE_QUEUE_LENGTH = 8;
    static constexpr uint8_t SEND_QUEUE_LENGTH = 8;

    // ESP-NOW Members
    uint8_t localMac[6];
    uint32_t localNodeId;
    bool espNowInitialized = false;

    // ESP-NOW Message structure (packed to avoid alignment padding)
    struct __attribute__((packed)) ESPNowMessage {
      uint8_t senderMac[6];
      uint32_t senderNodeId;
      uint16_t payloadLength;
      char payload[MAX_PAYLOAD_SIZE];
    };

    struct PendingMessage {
      uint32_t senderNodeId;
      char payload[MAX_PAYLOAD_SIZE + 1];
    };

    struct PendingTransmission {
      uint16_t length;
      uint8_t data[ESP_NOW_MAX_DATA_LEN];
    };

    static CommunicationService* instance; // For static callback
    QueueHandle_t receiveQueue = nullptr;
    QueueHandle_t sendQueue = nullptr;
    PendingTransmission activeTransmission;
    volatile bool sendInProgress = false;

    std::function<void(uint32_t)> connectionCallback = nullptr;
    std::function<void(uint32_t, JsonDocument, MessageType)> receivedControllerCallback = nullptr;

    ArrayList<GlowNode> nodes;

    // the CommunicationService will send periodic heartbeats to the other nodes to let them know it's still alive
    uint64_t last_hartbeat = 0;

    // Helper functions
    uint32_t macToNodeId(const uint8_t* mac);
    void macToString(const uint8_t* mac, char* buffer);
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
    static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);

    void receivedCallback(uint32_t from, String &msg);
    void broadcast(String message);
    void processSendQueue();
    void sendModeEvent(ModeEventKind kind, const String& title, const String& version,
                       const JsonDocument& payload, const String& command = "");

    void addNode(uint32_t id);
    uint16_t getNode(uint32_t id, GlowNode* node);
    uint32_t seenNode(uint32_t id);
    void removeNode(uint32_t id);
    bool updateNode(uint32_t id);
    void removeOldNodes();
    bool nodeExists(uint32_t id);

  public:
    CommunicationService();
    ~CommunicationService();

    void setup();
    void loop();

    ArrayList<GlowNode> getNodes();

    // communication
    void sendStateEvent(const JsonDocument& state);
    void sendModeCommand(const String& title, const String& version, const String& command,
                         const JsonDocument& payload);
    void sendSync(uint64_t timestamp);
    void sendWipe(uint16_t numberOfWipes);
    void sendDistanceUpdate(uint16_t distance, uint16_t level);

    uint32_t getNodeId();
    uint32_t getMeshTime();

    bool onNewConnection(std::function<void(uint32_t)> callback);
    bool onReceived(std::function<void(uint32_t, JsonDocument, MessageType)> callback);
};

#endif
