#include "LinkService.h"

LinkService::LinkService() {
}

LinkService::~LinkService() {
}

void LinkService::setup() {
  WiFi.mode(WIFI_STA);
  esp_now_init();

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BCAST, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void LinkService::loop() {
  if (millis() - this->lastHeartbeat > ESP_NOW_HEARTBEAT) {
    JsonDocument doc;
    doc["uptime"] = millis();

    this->send(BCAST, doc.as<String>(), LinkMessageType::HEARTBEAT);
    this->lastHeartbeat = millis();
  }
}

bool LinkService::send(const uint8_t* peer_addr, String data, LinkMessageType type) {
  JsonDocument doc;
  doc["type"] = type;
  doc["data"] = data;

  String jsonString;
  serializeJson(doc, jsonString);

  esp_err_t result = esp_now_send(peer_addr, (const uint8_t*)jsonString.c_str(), jsonString.length());
  return result == ESP_OK;
}

void LinkService::receive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  // Add peer if not already added
  if (!esp_now_is_peer_exist(info->src_addr)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, info->src_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      Serial.println("Added new peer");
    } else {
      Serial.println("Failed to add new peer");
      return;
    }

    this->addPeer(info->src_addr);
  }

  String msg;
  for (int i = 0; i < len; i++) {
    msg += (char)data[i];
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }

  JsonDocument innerData;
  DeserializationError innerError = deserializeJson(innerData, doc["data"].as<String>());
  if (innerError) {
    Serial.println("Failed to parse inner JSON");
    return;
  }

  LinkMessageType type = (LinkMessageType)doc["type"].as<int>();

  if (type == LinkMessageType::HEARTBEAT) {
    if (innerData["uptime"].is<uint64_t>()) {
      uint64_t uptime = innerData["uptime"].as<uint64_t>();
      this->updatePeer(info->src_addr, uptime);
    } else {
      this->updatePeer(info->src_addr);
    }

    JsonDocument doc;
    doc["uptime"] = millis();
    this->send(info->src_addr, doc.as<String>(), LinkMessageType::ECHO);
  } else if (type == LinkMessageType::ECHO) {
    if (innerData["uptime"].is<uint64_t>()) {
      uint64_t uptime = innerData["uptime"].as<uint64_t>();
      this->updatePeer(info->src_addr, uptime);
    } else {
      this->updatePeer(info->src_addr);
    }
  } else if (type == LinkMessageType::DATA) {
    if (this->dataHandler) {
      String response = this->dataHandler(innerData);

      // try parsing response as JSON
      JsonDocument responseDoc;
      DeserializationError responseError = deserializeJson(responseDoc, response);
      if (!responseError) {
        this->send(info->src_addr, responseDoc.as<String>(), LinkMessageType::DATA);
      }
    }
  } else if (type == LinkMessageType::COMMAND) {
    if (this->commandHandler) {
      String response = this->commandHandler(innerData);

      // try parsing response as JSON
      JsonDocument responseDoc;
      DeserializationError responseError = deserializeJson(responseDoc, response);
      if (!responseError && responseDoc["command"].is<int>()) {
        this->send(info->src_addr, responseDoc.as<String>(), LinkMessageType::COMMAND);
      }
    }
  } else {
    Serial.println("Received: UNKNOWN TYPE");
  }
}

void LinkService::addPeer(const uint8_t* peer_addr) {
  std::array<uint8_t, 6> addr;
  memcpy(addr.data(), peer_addr, 6);
  PeerInfo info = {0, millis()};
  this->peers[addr] = info;
  Serial.print("Peer added: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

void LinkService::updatePeer(const uint8_t* peer_addr) {
  std::array<uint8_t, 6> addr;
  memcpy(addr.data(), peer_addr, 6);
  if (this->peers.find(addr) != this->peers.end()) {
    this->peers[addr].lastSeen = millis();
  }
}

void LinkService::updatePeer(const uint8_t* peer_addr, uint64_t uptime) {
  std::array<uint8_t, 6> addr;
  memcpy(addr.data(), peer_addr, 6);
  if (this->peers.find(addr) != this->peers.end()) {
    this->peers[addr].uptime = uptime;
    this->peers[addr].lastSeen = millis();
  }
}

void LinkService::removePeer(const uint8_t* peer_addr) {
  std::array<uint8_t, 6> addr;
  memcpy(addr.data(), peer_addr, 6);
  this->peers.erase(addr);
  esp_now_del_peer(peer_addr);
  Serial.print("Peer removed: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

bool LinkService::isPeer(const uint8_t* peer_addr) {
  std::array<uint8_t, 6> addr;
  memcpy(addr.data(), peer_addr, 6);
  return this->peers.find(addr) != this->peers.end();
}

void LinkService::printPeers() {
  Serial.println("Known Peers:");
  for (const auto& [addr, info] : this->peers) {
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", addr[i]);
      if (i < 5) Serial.print(":");
    }
    double peer_uptime_now_s = ((double)info.uptime + (double)(millis() - info.lastSeen)) / 1000.0;
    double last_seen_s       =  (double)(millis() - info.lastSeen) / 1000.0;

    Serial.printf(" | Uptime: %.3f s | Last Seen: %.3f s ago\n",
                  peer_uptime_now_s,
                  last_seen_s);
  }
}

std::map<std::array<uint8_t, 6>, PeerInfo> LinkService::getPeers() {
  return this->peers;
}

void LinkService::setDataHandler(String (*f)(JsonDocument doc)) {
  this->dataHandler = f;
}

void LinkService::setCommandHandler(String (*f)(JsonDocument doc)) {
  this->commandHandler = f;
}
