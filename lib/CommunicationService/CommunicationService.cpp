#include "CommunicationService.h"


// Static instance for callback
CommunicationService* CommunicationService::instance = nullptr;

CommunicationService::CommunicationService() {
  // No scheduler needed anymore
}

CommunicationService::~CommunicationService() {
  if (this->espNowInitialized) {
    esp_now_deinit();
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

  // Send heartbeat
  if (millis() - this->last_hartbeat > HARTBEAT_INTERVAL) {
    this->last_hartbeat = millis();
    this->broadcast("{\"type\":2}");
    Serial.println("[DEBUG] Heartbeat sent");
  }

  // Remove old nodes
  this->removeOldNodes();
}

// communication functions
void CommunicationService::broadcast(String message) {
  if (!MESH_ON || !this->espNowInitialized) return;

  // Check message size
  if (message.length() > ESPNOW_MAX_PAYLOAD) {
    Serial.printf("[ERROR] Message too large: %d bytes (max %d)\n",
                  message.length(), ESPNOW_MAX_PAYLOAD);
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

  if (result == ESP_OK) {
    Serial.printf("[DEBUG] Broadcast sent: %s\n", message.c_str());
  } else {
    Serial.printf("[ERROR] Broadcast failed: %d\n", result);
  }
}

void CommunicationService::sendEvent(JsonDocument event) {
  if (!MESH_ON) return;

  JsonDocument message;

  message["type"] = MessageType::EVENT;
  message["message"] = event;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);

  Serial.println("[DEBUG] Event message sent");
}

void CommunicationService::sendSync(uint64_t timestamp) {
  if (!MESH_ON) return;
  
  JsonDocument message;

  message["type"] = MessageType::SYNC;
  message["message"]["timestamp"] = timestamp;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);

  Serial.println("[DEBUG] Sync message sent");
}

void CommunicationService::sendWipe(uint16_t numberOfWipes) {
  if (!MESH_ON) return;

  JsonDocument message;

  message["type"] = MessageType::WIPE;
  message["message"]["numberOfWipes"] = numberOfWipes;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);

  Serial.println("[DEBUG] Wipe message sent");
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
  if (instance == nullptr) return;

  Serial.printf("[DEBUG] ESP-NOW received %d bytes\n", len);

  // Validate message size
  if (len < 12) {  // Minimum header size
    Serial.printf("[ERROR] Received message too small: %d bytes\n", len);
    return;
  }

  // print debug info
  Serial.println("[DEBUG] Received raw data:");
  Serial.write(data, len);
  Serial.println();

  // Read header manually (no struct to avoid padding issues)
  uint8_t senderMac[6];
  uint32_t senderNodeId;
  uint16_t payloadLength;

  memcpy(senderMac, data, 6);
  memcpy(&senderNodeId, data + 6, 4);
  memcpy(&payloadLength, data + 10, 2);

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          senderMac[0], senderMac[1], senderMac[2],
          senderMac[3], senderMac[4], senderMac[5]);

  Serial.printf("[DEBUG] Header: MAC=%s, NodeID=%u, PayloadLen=%u\n",
                macStr, senderNodeId, payloadLength);

  // Validate sender MAC
  if (memcmp(mac, senderMac, 6) != 0) {
    Serial.println("[ERROR] MAC mismatch in received message");
    return;
  }

  // Validate payload length
  if (payloadLength > ESPNOW_MAX_PAYLOAD) {
    Serial.printf("[ERROR] Invalid payload length: %u (max %d)\n",
                  payloadLength, ESPNOW_MAX_PAYLOAD);
    return;
  }

  // Validate total message size
  if (len < 12 + payloadLength) {
    Serial.printf("[ERROR] Message truncated: expected %d bytes, got %d\n",
                  12 + payloadLength, len);
    return;
  }

  // Extract payload
  char payload[ESPNOW_MAX_PAYLOAD + 1];
  memcpy(payload, data + 12, payloadLength);
  payload[payloadLength] = '\0';

  Serial.printf("[DEBUG] Payload (%u bytes): %s\n", payloadLength, payload);

  // Create String and call receivedCallback
  String msgStr(payload);
  instance->receivedCallback(senderNodeId, msgStr);
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

  Serial.printf("[INFO] New GlowNode %u added\n", id);
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
      Serial.printf("[DEBUG] GlowNode %u removed (timeout)\n", this->nodes.get(i).id);
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

    Serial.printf("[INFO] GlowNode %u updated\n", id);

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

  Serial.printf("[DEBUG] Message received from %u: %s\n", from, msg.c_str());

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
  if (isNewNode && this->alertCallback != nullptr) {
    this->alertCallback();
  }

  // If heartbeat, ignore (already updated node)
  if (type == MessageType::HEARTBEAT) {
    Serial.println("[DEBUG] Heartbeat message received, ignoring message");
    return;
  }

  if (this->receivedControllerCallback == nullptr) {
    Serial.println("[ERROR] No callback for received message, ignoring message");
    return;
  }

  this->receivedControllerCallback(from, message, type);
}

bool CommunicationService::onNewConnection(std::function<void()> callback) {
  this->alertCallback = callback;

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
