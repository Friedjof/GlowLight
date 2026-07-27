#include "CommunicationService.h"

#ifndef GLOW_HEARTBEAT_INTERVAL
#define GLOW_HEARTBEAT_INTERVAL HARTBEAT_INTERVAL
#endif

// Static instance for callback
CommunicationService* CommunicationService::instance = nullptr;

CommunicationService::CommunicationService() {
  // No scheduler needed anymore
}

CommunicationService::~CommunicationService() {
  if (CommunicationService::instance == this) {
    CommunicationService::instance = nullptr;
  }

  if (this->espNowInitialized) {
    esp_now_deinit();
  }

  if (this->receiveQueue != nullptr) {
    vQueueDelete(this->receiveQueue);
  }
}

// main functions
void CommunicationService::setup() {
  if (!MESH_ON) {
    Serial.println("[INFO] Communication disabled");
    return;
  }

  // Set WiFi to station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Get local MAC address
  WiFi.macAddress(this->localMac);
  this->localNodeId = macToNodeId(this->localMac);

  char macStr[18];
  macToString(this->localMac, macStr);
  Serial.printf("[INFO] Local MAC: %s, NodeID: %u\n", macStr, this->localNodeId);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW initialization failed");
    return;
  }

  this->espNowInitialized = true;
  Serial.println("[INFO] ESP-NOW initialized");

  // Set static instance for callback
  CommunicationService::instance = this;

  this->receiveQueue = xQueueCreate(RECEIVE_QUEUE_LENGTH, sizeof(PendingMessage));
  if (this->receiveQueue == nullptr) {
    Serial.println("[ERROR] Failed to create ESP-NOW receive queue");
    CommunicationService::instance = nullptr;
    this->espNowInitialized = false;
    esp_now_deinit();
    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(CommunicationService::onDataRecv);

  // Add broadcast peer (one time)
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t broadcastPeer;
  memset(&broadcastPeer, 0, sizeof(broadcastPeer));
  memcpy(broadcastPeer.peer_addr, broadcastAddr, 6);
  broadcastPeer.channel = ESPNOW_CHANNEL;
  broadcastPeer.encrypt = false;

  if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
    Serial.println("[ERROR] Failed to add broadcast peer");
    return;
  }

  Serial.println("[INFO] CommunicationService initialized");
}

void CommunicationService::loop() {
  if (!MESH_ON || !this->espNowInitialized) return;

  PendingMessage pendingMessage;
  while (xQueueReceive(this->receiveQueue, &pendingMessage, 0) == pdTRUE) {
    String message(pendingMessage.payload);
    this->receivedCallback(pendingMessage.senderNodeId, message);
  }

  // Send heartbeat
  if (millis() - this->last_hartbeat > GLOW_HEARTBEAT_INTERVAL) {
    this->last_hartbeat = millis();
    this->broadcast("{\"type\":2}");
  }

  // Remove old nodes
  this->removeOldNodes();
}

// communication functions
void CommunicationService::broadcast(String message) {
  if (!MESH_ON || !this->espNowInitialized) return;

  // Check message size
  if (message.length() > MAX_PAYLOAD_SIZE) {
    Serial.printf("[ERROR] Message too large: %d bytes (max %d)\n",
                  message.length(), MAX_PAYLOAD_SIZE);
    return;
  }

  // Build ESP-NOW message
  ESPNowMessage msg;
  memcpy(msg.senderMac, this->localMac, 6);
  msg.senderNodeId = this->localNodeId;
  msg.payloadLength = message.length();
  memcpy(msg.payload, message.c_str(), msg.payloadLength);

  // Calculate actual message size
  size_t msgSize = sizeof(msg.senderMac) + sizeof(msg.senderNodeId) +
                   sizeof(msg.payloadLength) + msg.payloadLength;

  // Send broadcast
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)&msg, msgSize);

  if (result != ESP_OK) {
    Serial.printf("[ERROR] Broadcast failed: %d\n", result);
  }
}

void CommunicationService::sendModeEvent(ModeEventKind kind, const String& title,
                                         const String& version, const JsonDocument& payload,
                                         const String& command) {
  if (!MESH_ON) return;

  JsonDocument event;

  event["type"] = MessageType::EVENT;
  event["message"]["eventKind"] = static_cast<uint8_t>(kind);
  event["message"]["mode"]["title"] = title;
  event["message"]["mode"]["version"] = version;
  event["message"]["payload"] = payload;

  if (kind == ModeEventKind::COMMAND) {
    event["message"]["command"] = command;
  }

  String msg;
  serializeJson(event, msg);

  this->broadcast(msg);
}

void CommunicationService::sendStateEvent(const JsonDocument& state) {
  String title = state["title"].as<String>();
  String version = state["version"].as<String>();
  JsonObjectConst registry = state["registry"].as<JsonObjectConst>();

  if (title.isEmpty() || version.isEmpty() || registry.isNull()) {
    Serial.println("[ERROR] Invalid mode state, event not sent");
    return;
  }

  JsonDocument payload;
  payload.set(registry);

  this->sendModeEvent(ModeEventKind::STATE, title, version, payload);
}

void CommunicationService::sendModeCommand(const String& title, const String& version,
                                           const String& command,
                                           const JsonDocument& payload) {
  if (title.isEmpty() || version.isEmpty() || command.isEmpty() ||
      !payload.is<JsonObject>()) {
    Serial.println("[ERROR] Invalid mode command, event not sent");
    return;
  }

  this->sendModeEvent(ModeEventKind::COMMAND, title, version, payload, command);
}

void CommunicationService::sendSync(uint64_t timestamp) {
  if (!MESH_ON) return;
  
  JsonDocument message;

  message["type"] = MessageType::SYNC;
  message["message"]["timestamp"] = timestamp;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);
}

void CommunicationService::sendWipe(uint16_t numberOfWipes) {
  if (!MESH_ON) return;

  JsonDocument message;

  message["type"] = MessageType::WIPE;
  message["message"]["numberOfWipes"] = numberOfWipes;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);
}

void CommunicationService::sendDistanceUpdate(uint16_t distance, uint16_t level) {
  if (!MESH_ON) return;

  JsonDocument message;

  message["type"] = MessageType::LEVEL;
  message["message"]["distance"] = distance;
  message["message"]["level"] = level;
  message["message"]["active"] = true;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);
}

// Helper functions
uint32_t CommunicationService::macToNodeId(const uint8_t* mac) {
  uint32_t id = 0;
  id |= ((uint32_t)mac[3] << 24);
  id |= ((uint32_t)mac[4] << 16);
  id |= ((uint32_t)mac[5] << 8);
  id |= (uint8_t)(mac[0] ^ mac[1] ^ mac[2]);
  return id;
}

void CommunicationService::macToString(const uint8_t* mac, char* buffer) {
  sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void CommunicationService::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (instance == nullptr || instance->receiveQueue == nullptr) return;

  // Validate message size
  if (len < MESSAGE_HEADER_SIZE || len > ESP_NOW_MAX_DATA_LEN) {
    return;
  }

  // Read header manually (no struct to avoid padding issues)
  uint8_t senderMac[6];
  uint32_t senderNodeId;
  uint16_t payloadLength;

  memcpy(senderMac, data, 6);
  memcpy(&senderNodeId, data + 6, 4);
  memcpy(&payloadLength, data + 10, 2);

  // Validate sender MAC
  if (memcmp(mac, senderMac, 6) != 0) {
    return;
  }

  // Validate payload length
  if (payloadLength > MAX_PAYLOAD_SIZE) {
    return;
  }

  // Validate total message size
  if (len < MESSAGE_HEADER_SIZE + payloadLength) {
    return;
  }

  PendingMessage pendingMessage;
  pendingMessage.senderNodeId = senderNodeId;
  memcpy(pendingMessage.payload, data + MESSAGE_HEADER_SIZE, payloadLength);
  pendingMessage.payload[payloadLength] = '\0';

  xQueueSend(instance->receiveQueue, &pendingMessage, 0);
}

// manage nodes
void CommunicationService::addNode(uint32_t id) {
  GlowNode newNode = { id, millis() };

  // TODO: check if max nodes reached because of memory (not necessary for now)

  if (this->nodes.size() == 0) {
    this->nodes.add(newNode);
  } else {
    for (int i = 0; i < this->nodes.size(); i++) {
      if (this->nodes.get(i).id == id) return;
    }

    this->nodes.add(newNode);
  }
}

uint16_t CommunicationService::getNode(uint32_t id, GlowNode* node) {
  for (int i = 0; i < this->nodes.size(); i++) {
    if (this->nodes.get(i).id == id) {
      *node = this->nodes.get(i);
      return i;
    }
  }

  return -1;
}

uint32_t CommunicationService::seenNode(uint32_t id) {
  for (int i = 0; i < this->nodes.size(); i++) {
    if (this->nodes.get(i).id == id) {
      return this->nodes.get(i).lastSeen;
    }
  }

  return 0;
}

void CommunicationService::removeNode(uint32_t id) {
  for (int i = 0; i < this->nodes.size(); i++) {
    if (this->nodes.get(i).id == id) {
      this->nodes.remove(i);
      break;
    }
  }
}

void CommunicationService::removeOldNodes() {
  for (int i = 0; i < this->nodes.size(); i++) {
    if (millis() - this->nodes.get(i).lastSeen > GLOW_NODE_TIMEOUT) {
      this->nodes.remove(i--);
    }
  }
}

bool CommunicationService::updateNode(uint32_t id) {
  if(this->nodeExists(id)) {
    GlowNode node;
    uint16_t index = this->getNode(id, &node);
    node.lastSeen = millis();

    // this might raise a warning (see the issue https://github.com/braydenanderson2014/C-Arduino-Libraries/issues/89)
    this->nodes.set(index, node);

    return true;
  }

  this->addNode(id);
  return false;
}

bool CommunicationService::nodeExists(uint32_t id) {
  for (int i = 0; i < this->nodes.size(); i++) {
    if (this->nodes.get(i).id == id) {
      return true;
    }
  }

  return false;
}

// callbacks
void CommunicationService::receivedCallback(uint32_t from, String &msg) {
  // Ignore messages from self
  if (from == this->localNodeId) {
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print("[ERROR] deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  MessageType type = doc["type"];
  JsonDocument message = doc["message"];

  if (type >= static_cast<int>(MessageType::MAX)) {
    Serial.println("[ERROR] Invalid message type, ignoring message");
    return;
  }

  // Update node (auto-discovery happens here)
  bool isNewNode = !this->updateNode(from);

  // Call callback for new nodes
  if (isNewNode) {
    Serial.printf("[INFO] Discovered node %u\n", from);

    if (this->connectionCallback != nullptr) {
      this->connectionCallback(from);
    }
  }

  // If heartbeat, ignore (already updated node)
  if (type == MessageType::HEARTBEAT) {
    return;
  }

  if (this->receivedControllerCallback == nullptr) {
    Serial.println("[ERROR] No callback for received message, ignoring message");
    return;
  }

  this->receivedControllerCallback(from, message, type);
}

bool CommunicationService::onNewConnection(std::function<void(uint32_t)> callback) {
  this->connectionCallback = callback;

  return true;
}

uint32_t CommunicationService::getNodeId() {
  if (!MESH_ON) {
    return 0;
  }

  return this->localNodeId;
}

uint32_t CommunicationService::getMeshTime() {
  if (!MESH_ON) {
    return millis();
  }

  return millis();  // Use local time instead of mesh time
}

bool CommunicationService::onReceived(std::function<void(uint32_t, JsonDocument, MessageType)> callback) {
  this->receivedControllerCallback = callback;

  return true;
}
