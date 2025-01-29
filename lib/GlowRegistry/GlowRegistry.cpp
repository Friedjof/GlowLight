#include "GlowRegistry.h"

GlowRegistry::GlowRegistry() {
}

// meta functions
void GlowRegistry::setTitle(String title) {
  this->meta["title"] = title;
}

void GlowRegistry::setVersion(String version) {
  this->meta["version"] = version;
}

String GlowRegistry::getTitle() {
  if (!this->meta["title"].is<String>()) {
    return "";
  }

  return this->meta["title"];
}

String GlowRegistry::getVersion() {
  if (!this->meta["version"].is<String>()) {
    return "";
  }

  return this->meta["version"];
}

bool GlowRegistry::hasTitle() {
  return this->meta["title"].is<String>();
}

bool GlowRegistry::hasVersion() {
  return this->meta["version"].is<String>();
}

// helper functions
String GlowRegistry::CRGB2Hex(CRGB color) {
  char hex[7];
  sprintf(hex, "%02X%02X%02X", color.r, color.g, color.b);
  return String(hex);
}

CRGB GlowRegistry::Hex2CRGB(String hex) {
  if (hex.length() != 6) {
    return CRGB(0, 0, 0);
  }

  char r[3];
  char g[3];
  char b[3];

  hex.substring(0, 2).toCharArray(r, 3);
  hex.substring(2, 4).toCharArray(g, 3);
  hex.substring(4, 6).toCharArray(b, 3);

  return CRGB(strtol(r, NULL, 16), strtol(g, NULL, 16), strtol(b, NULL, 16));
}

// init functions
bool GlowRegistry::init(String key, RegistryType type) {
  if (type == RegistryType::INT) {
    return this->init(key, type, uint16_t(0));
  } else if (type == RegistryType::STRING) {
    return this->init(key, type, "");
  } else if (type == RegistryType::BOOL) {
    return this->init(key, type, false);
  } else if (type == RegistryType::COLOR) {
    return this->init(key, type, CRGB(0, 0, 0));
  } else {
    return false;
  }
}

bool GlowRegistry::init(String key, RegistryType type, uint16_t defaultValue) {
  return this->init(key, type, defaultValue, uint16_t(0), uint16_t(-1));
}

bool GlowRegistry::init(String key, RegistryType type, uint16_t defaultValue, uint16_t min, uint16_t max) {
  if (this->contains(key)) {
    Serial.println("[ERROR] Key already initialized");
    return false;
  }

  this->meta[key]["type"] = type;
  this->meta[key]["min"] = min;
  this->meta[key]["max"] = max;
  this->meta[key]["default"] = defaultValue;
  
  this->registry[key] = defaultValue;

  Serial.printf("[DEBUG] Initialized key '%s' with default value %d\n", key.c_str(), defaultValue);

  return this->registry[key] == defaultValue;
}

bool GlowRegistry::init(String key, RegistryType type, String defaultValue) {
  if (this->contains(key)) {
    Serial.println("[ERROR] Key already initialized");
    return false;
  }

  this->meta[key]["type"] = type;
  this->meta[key]["default"] = defaultValue;
  
  this->registry[key] = defaultValue;

  Serial.printf("[DEBUG] Initialized key '%s' with value '%s'\n", key.c_str(), defaultValue.c_str());

  return this->registry[key] == defaultValue;
}

bool GlowRegistry::init(String key, RegistryType type, bool defaultValue) {
  if (this->contains(key)) {
    Serial.println("[ERROR] Key already initialized");
    return false;
  }

  this->meta[key]["type"] = type;
  this->meta[key]["default"] = defaultValue;
  
  this->registry[key] = defaultValue;

  Serial.printf("[DEBUG] Initialized key '%s' with default value %s\n", key.c_str(), defaultValue ? "true" : "false");

  return this->registry[key] == defaultValue;
}

bool GlowRegistry::init(String key, RegistryType type, CRGB defaultValue) {
  if (this->contains(key)) {
    Serial.println("[ERROR] Key already initialized");
    return false;
  }

  this->meta[key]["type"] = type;
  this->meta[key]["default"] = this->CRGB2Hex(defaultValue);
  
  this->registry[key] = this->CRGB2Hex(defaultValue);

  Serial.printf("[DEBUG] Initialized key '%s' with default value %s\n", key.c_str(), this->CRGB2Hex(defaultValue).c_str());

  return this->registry[key] == this->CRGB2Hex(defaultValue);
}

// get functions
uint16_t GlowRegistry::getInt(String key) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return 0;
  }

  return this->registry[key];
}

String GlowRegistry::getString(String key) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return "";
  }

  return this->registry[key].as<String>();
}

bool GlowRegistry::getBool(String key) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  return this->registry[key].as<bool>();
}

CRGB GlowRegistry::getColor(String key) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return CRGB(0, 0, 0);
  }

  return this->Hex2CRGB(this->registry[key].as<String>());
}

// set functions
bool GlowRegistry::setInt(String key, uint16_t value) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  uint16_t min = this->meta[key]["min"];
  uint16_t max = this->meta[key]["max"];

  if (value < min || value > max) {
    Serial.println("[ERROR] Value out of bounds");
    return false;
  }

  this->registry[key] = value;

  return this->registry[key] == value;
}

bool GlowRegistry::setString(String key, String value) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  this->registry[key] = value;

  return this->registry[key] == value;
}

bool GlowRegistry::setBool(String key, bool value) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  this->registry[key] = value;

  return this->registry[key] == value;
}

bool GlowRegistry::setColor(String key, CRGB value) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  this->registry[key] = this->CRGB2Hex(value);

  return this->registry[key] == this->CRGB2Hex(value);
}

// other functions
bool GlowRegistry::reset(String key) {
  if (!this->contains(key)) {
    Serial.printf("[ERROR] Key not initialized: %s\n", key.c_str());
    return false;
  }

  this->registry[key] = this->meta[key]["default"];

  return this->registry[key] == this->meta[key]["default"];
}

uint16_t GlowRegistry::size() {
  return this->registry.size();
}

bool GlowRegistry::contains(String key) {
  return this->meta[key].is<JsonObject>();
}

// serialize and deserialize
JsonDocument GlowRegistry::serialize() {
  JsonDocument serialized;

  serialized["registry"] = this->registry;
  serialized["title"] = this->meta["title"];
  serialized["version"] = this->meta["version"];

  return serialized;
}

bool GlowRegistry::deserialize(JsonDocument doc) {
  // check if the title and version match (to prevent deserialization of wrong data for another mode)
  if (doc["title"].as<String>() != this->meta["title"]) {
    Serial.print("[ERROR] The title '");
    Serial.print(doc["title"].as<String>());
    Serial.print("' does not match with this title '");
    Serial.print(this->meta["title"].as<String>());
    Serial.println("'. Skipping deserialization");
    return false;
  } else if (doc["version"].as<String>() != this->meta["version"]) {
    Serial.print("[ERROR] The version '");
    Serial.print(doc["version"].as<String>());
    Serial.print("' from mode '");
    Serial.print(doc["title"].as<String>());
    Serial.print("' does not match with this version '");
    Serial.print(this->meta["version"].as<String>());
    Serial.println("'. Skipping deserialization");
    return false;
  }

  JsonObject reg = doc["registry"].as<JsonObject>();

  for (JsonPair kv : this->registry.as<JsonObject>()) {
    String key = kv.key().c_str();

    RegistryType type = this->meta[key]["type"];

    if (!reg[key].isNull()) {
      if (type == RegistryType::INT) {
        if (!this->setInt(key, reg[key].as<uint16_t>())) return false;
      } else if (type == RegistryType::STRING) {
        if (!this->setString(key, reg[key].as<String>())) return false;
      } else if (type == RegistryType::BOOL) {
        if (!this->setBool(key, reg[key].as<bool>())) return false;
      } else if (type == RegistryType::COLOR) {
        if (!this->setColor(key, this->Hex2CRGB(reg[key].as<String>()))) return false;
      } else {
        Serial.println("[ERROR] Invalid type");
        return false;
      }
    } else {
      Serial.print("[ERROR] Key '");
      Serial.print(key);
      Serial.println("' not found in document");
    }
  }

  return true;
}
