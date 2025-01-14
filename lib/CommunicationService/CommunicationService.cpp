#include "CommunicationService.h"


CommunicationService::CommunicationService(Scheduler* scheduler) {
  this->scheduler = scheduler;
}

CommunicationService::~CommunicationService() {
  if (this->mesh != nullptr) {
    delete this->mesh;
  }
}

void CommunicationService::setup() {
  // Mesh-Netzwerk initialized
  if (this->mesh != nullptr) {
    this->mesh->init(MESH_PREFIX, MESH_PASSWORD, this->scheduler, MESH_PORT);

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
}

void CommunicationService::send(String message, GlowNode node) {
  if (this->mesh != nullptr) {
    this->mesh->sendSingle(node.id, message);
    Serial.printf("[INFO] Message sent to %u: %s\n", node.id, message.c_str());
  } else {
    Serial.println("[ERROR] Mesh is not initialized");
  }
}

void CommunicationService::broadcast(String message) {
  if (mesh != nullptr) {
    mesh->sendBroadcast(message);
    Serial.printf("[INFO] Broadcast message sent: %s\n", message.c_str());
  } else {
    Serial.println("[ERROR] Mesh is not initialized");
  }
}

ArrayList<GlowNode> CommunicationService::getNodes() {
  return nodes;
}

void CommunicationService::receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[INFO] Message received from %u: %s\n", from, msg.c_str());
}

void CommunicationService::newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[INFO] New connection: %u\n", nodeId);

  if (this->alertCallback != nullptr) {
    this->alertCallback();
  }

  GlowNode newNode = { nodeId, millis() };
}

void CommunicationService::changedConnectionsCallback() {
  Serial.println("[INFO] Connections changed");
}

bool CommunicationService::setNewConnectionCallback(std::function<void()> callback) {
  this->alertCallback = callback;

  return true;
}
