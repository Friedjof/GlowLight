#include "CommunicationService.h"


CommunicationService::CommunicationService(Scheduler* scheduler) {
  this->scheduler = scheduler;
}

CommunicationService::~CommunicationService() {
  if (this->mesh != nullptr) {
    delete this->mesh;
  }
}

// main functions
void CommunicationService::setup() {
  // Mesh-Netzwerk initialized
  if (this->mesh != nullptr) {
    this->mesh->init(MESH_PREFIX, MESH_PASSWORD, this->scheduler, MESH_PORT);

    // set rtc time to 0
    this->setTimestamp(0);
    Serial.println("[INFO] RTC time initialized");

    // initialize mesh
    this->mesh->onReceive(std::bind(&CommunicationService::receivedCallback, this, std::placeholders::_1, std::placeholders::_2));
    this->mesh->onNewConnection(std::bind(&CommunicationService::newConnectionCallback, this, std::placeholders::_1));
    this->mesh->onChangedConnections(std::bind(&CommunicationService::changedConnectionsCallback, this));
  }

  Serial.println("[INFO] CommunicationService initialized");
}

void CommunicationService::loop() {
  if (this->mesh != nullptr) {
    this->mesh->update();
  }

  this->removeOldNodes();
}

// communication functions
void CommunicationService::send(String message, GlowNode node) {
  if (this->mesh != nullptr) {
    this->mesh->sendSingle(node.id, message);
    Serial.printf("[INFO] Message sent to %u: %s\n", node.id, message.c_str());
  } else {
    Serial.println("[ERROR] Mesh is not initialized");
  }
}

void CommunicationService::broadcast(String message) {
  if (this->nodes.size() == 0) {
    Serial.println("[DEBUG] No nodes available, cannot broadcast message");
    return;
  }

  if (mesh != nullptr) {
    mesh->sendBroadcast(message);
    Serial.printf("[DEBUG] Broadcast message sent: %s\n", message.c_str());
  } else {
    Serial.println("[ERROR] Mesh is not initialized");
  }
}

void CommunicationService::sendEvent(JsonDocument event) {
  JsonDocument message;

  message["type"] = MessageType::EVENT;
  message["message"] = event;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);

  Serial.println("[DEBUG] Event message sent");
}

void CommunicationService::sendSync(uint64_t timestamp) {
  JsonDocument message;

  message["type"] = MessageType::SYNC;
  message["message"]["timestamp"] = timestamp;

  String msg;
  serializeJson(message, msg);

  this->broadcast(msg);

  Serial.println("[DEBUG] Sync message sent");
}

// time functions
uint64_t CommunicationService::getTimestamp() {
  return (uint64_t)time(nullptr);
}

void CommunicationService::setTimestamp(uint64_t timestamp) {
  struct timeval tv = { static_cast<time_t>(timestamp), 0 };
  settimeofday(&tv, nullptr);
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
  //Serial.printf("[DEBUG] Message received from %u: %s\n", from, msg.c_str());

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

  if (this->receivedControllerCallback != nullptr) {
    this->receivedControllerCallback(from, message, type);
  } else {
    Serial.println("[ERROR] No callback for received message, ignoring message");
  }

  this->updateNode(from);
}

void CommunicationService::newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[INFO] GlowNode connected: %u\n", nodeId);

  if (this->alertCallback != nullptr && !this->updateNode(nodeId)) {
    this->alertCallback();
  }
}

void CommunicationService::changedConnectionsCallback() {
  Serial.println("[INFO] Connections changed");
}

bool CommunicationService::onNewConnection(std::function<void()> callback) {
  this->alertCallback = callback;

  return true;
}

bool CommunicationService::onReceived(std::function<void(uint32_t, JsonDocument, MessageType)> callback) {
  this->receivedControllerCallback = callback;

  return true;
}
