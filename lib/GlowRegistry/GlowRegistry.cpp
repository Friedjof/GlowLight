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
  if (!this->isHexColor(hex)) {
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

String GlowRegistry::normalizeHexColor(const String& value) {
  String candidate = value;
  candidate.trim();
  if (candidate.startsWith("#")) candidate = candidate.substring(1);
  if (!this->isHexColor(candidate)) return String("");
  candidate.toUpperCase();
  return candidate;
}

bool GlowRegistry::isHexColor(const String& hex) {
  if (hex.length() != 6) return false;
  for (size_t i = 0; i < hex.length(); ++i) {
    char character = hex[i];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f') ||
          (character >= 'A' && character <= 'F'))) return false;
  }
  return true;
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

  if (min > max) {
    Serial.println("[WARNING] Min is greater than max, swapping values");
    uint16_t tmp = min;
    min = max;
    max = tmp;
  }

  this->meta[key]["type"] = type;
  this->meta[key]["min"] = min;
  this->meta[key]["max"] = max;
  this->meta[key]["default"] = defaultValue;
  this->meta[key]["writable"] = true;
  
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
  this->meta[key]["writable"] = true;
  
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
  this->meta[key]["writable"] = true;
  
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
  this->meta[key]["writable"] = true;
  
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
    Serial.printf("[ERROR] Value %d out of range [%d, %d]\n", value, min, max);
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

bool GlowRegistry::setWritable(String key, bool writable) {
  if (!this->contains(key)) return false;
  this->meta[key]["writable"] = writable;
  return true;
}

bool GlowRegistry::valueValid(const String& key, JsonVariantConst value) {
  if (!this->contains(key) || value.isNull()) return false;

  RegistryType type = this->meta[key]["type"];
  if (type == RegistryType::INT) {
    if (!value.is<int>()) return false;
    int candidate = value.as<int>();
    uint16_t minimum = this->meta[key]["min"];
    uint16_t maximum = this->meta[key]["max"];
    return candidate >= minimum && candidate <= maximum;
  }
  if (type == RegistryType::STRING) return value.is<String>();
  if (type == RegistryType::BOOL) return value.is<bool>();
  if (type == RegistryType::COLOR) {
    return value.is<String>() && !this->normalizeHexColor(value.as<String>()).isEmpty();
  }
  return false;
}

bool GlowRegistry::applyValue(const String& key, JsonVariantConst value) {
  RegistryType type = this->meta[key]["type"];
  if (type == RegistryType::INT) return this->setInt(key, value.as<uint16_t>());
  if (type == RegistryType::STRING) return this->setString(key, value.as<String>());
  if (type == RegistryType::BOOL) return this->setBool(key, value.as<bool>());
  if (type == RegistryType::COLOR) {
    return this->setColor(key, this->Hex2CRGB(
        this->normalizeHexColor(value.as<String>())));
  }
  return false;
}

bool GlowRegistry::setValue(String key, JsonVariantConst value) {
  if (!this->contains(key) || !this->meta[key]["writable"].as<bool>() ||
      !this->valueValid(key, value)) return false;
  return this->applyValue(key, value);
}

// serialize and deserialize
JsonDocument GlowRegistry::serialize() {
  JsonDocument serialized;

  serialized["registry"] = this->registry;
  serialized["title"] = this->meta["title"];
  serialized["version"] = this->meta["version"];

  return serialized;
}

JsonDocument GlowRegistry::describe() {
  JsonDocument description;
  JsonObject settings = description.to<JsonObject>();

  for (JsonPair setting : this->registry.as<JsonObject>()) {
    String key = setting.key().c_str();
    JsonObject descriptor = settings[key].to<JsonObject>();
    RegistryType type = this->meta[key]["type"];
    if (type == RegistryType::INT) {
      descriptor["type"] = "integer";
      descriptor["minimum"] = this->meta[key]["min"];
      descriptor["maximum"] = this->meta[key]["max"];
    } else if (type == RegistryType::STRING) {
      descriptor["type"] = "string";
    } else if (type == RegistryType::BOOL) {
      descriptor["type"] = "boolean";
    } else if (type == RegistryType::COLOR) {
      descriptor["type"] = "string";
      descriptor["format"] = "rgb-hex";
    }
    descriptor["default"] = this->meta[key]["default"];
    descriptor["writable"] = this->meta[key]["writable"];
  }
  return description;
}

bool GlowRegistry::deserialize(const JsonDocument& doc) {
  // AbstractMode validates stable mode ID and state schema version. Display
  // title and implementation version are deliberately not protocol identities.
  if (!doc["registry"].is<JsonObjectConst>()) return false;

  JsonObjectConst reg = doc["registry"].as<JsonObjectConst>();

  if (reg.size() != this->registry.size()) {
    Serial.println("[ERROR] Registry field count does not match");
    return false;
  }

  // Validate the complete document before changing any value.
  for (JsonPair kv : this->registry.as<JsonObject>()) {
    String key = kv.key().c_str();
    if (!this->valueValid(key, reg[key])) {
      Serial.print("[ERROR] Key '");
      Serial.print(key);
      Serial.println("' is missing or invalid");
      return false;
    }
  }

  for (JsonPairConst incoming : reg) {
    if (!this->contains(incoming.key().c_str())) return false;
  }

  for (JsonPair kv : this->registry.as<JsonObject>()) {
    String key = kv.key().c_str();
    if (!this->applyValue(key, reg[key])) return false;
  }

  return true;
}
