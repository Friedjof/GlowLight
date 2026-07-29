#include "PreferencesConfigStore.h"

uint32_t PreferencesConfigStore::crc32(const String& value) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < value.length(); ++i) {
    crc ^= static_cast<uint8_t>(value[i]);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      uint32_t mask = -(crc & 1U);
      crc = (crc >> 1) ^ (0xedb88320U & mask);
    }
  }
  return ~crc;
}

PreferencesConfigStore::Slot PreferencesConfigStore::decode(const String& stored) {
  Slot slot;
  if (stored.isEmpty()) return slot;
  JsonDocument wrapper;
  if (deserializeJson(wrapper, stored) ||
      wrapper["format"].as<String>() != "glow.nvs/1" ||
      !wrapper["generation"].is<uint64_t>() ||
      !wrapper["payload"].is<String>() || !wrapper["crc32"].is<uint32_t>()) {
    return slot;
  }
  String payload = wrapper["payload"].as<String>();
  uint64_t generation = wrapper["generation"].as<uint64_t>();
  if (crc32(checksumInput(payload, generation)) !=
      wrapper["crc32"].as<uint32_t>()) return slot;
  String error;
  if (!DeviceConfig::deserialize(payload, &slot.config, &error)) return slot;
  slot.generation = generation;
  slot.valid = slot.generation != 0;
  return slot;
}

String PreferencesConfigStore::checksumInput(const String& payload,
                                             uint64_t generation) {
  char generationText[21];
  snprintf(generationText, sizeof(generationText), "%llu",
           static_cast<unsigned long long>(generation));
  String input = "glow.nvs/1:";
  input.concat(generationText);
  input.concat(":");
  input.concat(payload.c_str());
  return input;
}

String PreferencesConfigStore::encode(const DeviceConfig& config,
                                      uint64_t generation) {
  String payload = config.serialize();
  JsonDocument wrapper;
  wrapper["format"] = "glow.nvs/1";
  wrapper["generation"] = generation;
  wrapper["payload"] = payload;
  wrapper["crc32"] = crc32(checksumInput(payload, generation));
  String stored;
  serializeJson(wrapper, stored);
  return stored;
}

bool PreferencesConfigStore::load(DeviceConfig* output) {
  if (output == nullptr) return false;
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, true)) return false;
  Slot first = decode(preferences.getString(SLOT_A, ""));
  Slot second = decode(preferences.getString(SLOT_B, ""));
  preferences.end();
  if (!first.valid && !second.valid) return false;
  *output = !second.valid || (first.valid && first.generation >= second.generation)
                ? first.config
                : second.config;
  return true;
}

bool PreferencesConfigStore::save(const DeviceConfig& config) {
  String error;
  if (!config.validate(&error)) return false;
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return false;
  Slot first = decode(preferences.getString(SLOT_A, ""));
  Slot second = decode(preferences.getString(SLOT_B, ""));
  uint64_t latest = max(first.generation, second.generation);
  if (latest == UINT64_MAX) {
    preferences.end();
    return false;
  }
  uint64_t generation = latest + 1;
  const char* target = !first.valid
                           ? SLOT_A
                           : (!second.valid || first.generation > second.generation
                                  ? SLOT_B
                                  : SLOT_A);
  String stored = encode(config, generation);
  bool written = preferences.putString(target, stored) == stored.length();
  bool verified = written && decode(preferences.getString(target, "")).valid;
  preferences.end();
  return verified;
}

bool PreferencesConfigStore::clear() {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return false;
  bool cleared = preferences.clear();
  preferences.end();
  return cleared;
}
