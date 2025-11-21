#include "GlowRegistry.h"

GlowRegistry::GlowRegistry(LinkService* linkService) {
  this->linkService = linkService;
}

GlowRegistry::~GlowRegistry() { }

void GlowRegistry::setup() {
  Serial.println("[INFO] GlowRegistry setup complete");
}

void GlowRegistry::loop() {
}

void GlowRegistry::set(String nameSpace, String key, int value, bool announce) {
  if (!this->_global[nameSpace].is<JsonObject>()) {
    this->_global[nameSpace] = JsonObject();
  }
  if (this->_global[nameSpace][key].is<int>() && this->_global[nameSpace][key].as<int>() == value) {
    return;
  }

  this->_global[nameSpace][key] = value;

  if (!announce) {
    return;
  }

  JsonDocument doc;
  doc["type"] = RegistryMessageType::SET;
  doc["namespace"] = nameSpace;
  doc["key"] = key;
  doc["value"] = value;

  for (const auto& [addr, info] : this->linkService->getPeers()) {
    this->linkService->send(addr.data(), doc.as<String>(), LinkMessageType::DATA);
  }
}

int GlowRegistry::get(String nameSpace, String key) {
  int value = -1;
  if (this->_global[nameSpace].is<JsonObject>() && this->_global[nameSpace][key].is<int>()) {
    value = this->_global[nameSpace][key].as<int>();
  } else {
    // TODO: Request value from other devices if not found locally
  }

  return value;
}

void GlowRegistry::set(String key, int value, bool announce) {
  if (this->activeNamespace == "") {
    Serial.println("[WARN] GlowRegistry: No active namespace set");
    return;
  }

  this->set(this->activeNamespace, key, value, announce);
}

int GlowRegistry::get(String key) {
  if (this->activeNamespace == "") {
    Serial.println("[WARN] GlowRegistry: No active namespace set");
    return -1;
  }

  return this->get(this->activeNamespace, key);
}

void GlowRegistry::setPrivateInt(String key, int value) {
  if (this->_private[key].is<int>() && this->_private[key].as<int>() == value) {
    return;
  }

  this->_private[key] = value;
}

void GlowRegistry::setPrivateU64(String key, uint64_t value) {
  if (this->_private[key].is<uint64_t>() && this->_private[key].as<uint64_t>() == value) {
    return;
  }

  this->_private[key] = value;
}

void GlowRegistry::setPrivateStr(String key, String value) {
  if (this->_private[key].is<String>() && this->_private[key].as<String>() == value) {
    return;
  }

  this->_private[key] = value;
}

int GlowRegistry::getPrivateInt(String key) {
  int value = -1;
  if (this->_private[key].is<int>()) {
    value = this->_private[key].as<int>();
  }
  return value;
}

uint64_t GlowRegistry::getPrivateU64(String key) {
  uint64_t value = -1;
  if (this->_private[key].is<uint64_t>()) {
    value = this->_private[key].as<uint64_t>();
  }
  return value;
}

String GlowRegistry::getPrivateStr(String key) {
  String value = "";
  if (this->_private[key].is<String>()) {
    value = this->_private[key].as<String>();
  }
  return value;
}

void GlowRegistry::setActive(String nameSpace, bool announce) {
  if (nameSpace == "" || nameSpace == this->activeNamespace) {
    return;
  }

  this->activeNamespace = nameSpace;

  if (!announce) {
    return;
  }

  JsonDocument doc;
  doc["command"] = Command::NAMESPACE;
  doc["namespace"] = nameSpace;

  for (const auto& [addr, info] : this->linkService->getPeers()) {
    this->linkService->send(addr.data(), doc.as<String>(), LinkMessageType::COMMAND);
  }
}

String GlowRegistry::getActive() {
  return this->activeNamespace;
}

String GlowRegistry::receive(JsonDocument data) {
  if (!data["type"].is<int>()) {
    Serial.println("[WARN] GlowRegistry: Received data without type");
    return "";
  }

  RegistryMessageType type = static_cast<RegistryMessageType>(data["type"].as<int>());
  if (type == RegistryMessageType::SET) {

    if (!data["namespace"].is<String>() || !data["key"].is<String>() || !data["value"].is<int>()) {
      Serial.println("[WARN] GlowRegistry: Received SET without namespace, key or value");
      return "";
    }

    String nameSpace = data["namespace"].as<String>();
    String key = data["key"].as<String>();
    int value = data["value"].as<int>();

    this->set(nameSpace, key, value, false);
    Serial.printf("[INFO] GlowRegistry: SET %s.%s = %d\n", nameSpace.c_str(), key.c_str(), value);
  } else if (type == RegistryMessageType::GET) {
    // TODO: Implement GET handling
  } else {
    Serial.println("[WARN] GlowRegistry: Received data with unknown type");
  }

  return "";
}

String GlowRegistry::sync() {
  JsonDocument doc;
  doc["command"] = Command::ACK;
  doc["callback"] = Command::SYNC;
  doc["namespace"] = this->activeNamespace;
  doc["data"] = this->_global[this->activeNamespace].as<JsonObject>();

  return doc.as<String>();
}

void GlowRegistry::syncNamespace(String nameSpace, JsonObject data) {
  if (data.isNull()) {
    return;
  }

  for (JsonPair kv : data) {
    String key = kv.key().c_str();
    int value = -1;
    if (kv.value().is<int>()) {
      value = kv.value().as<int>();
    } else {
      continue;
    }

    this->set(nameSpace, key, value, false);
    Serial.printf("[INFO] GlowRegistry: SYNC %s.%s = %d\n", nameSpace.c_str(), key.c_str(), value);
  }
}