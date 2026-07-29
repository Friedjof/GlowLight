#ifndef PREFERENCESCONFIGSTORE_H
#define PREFERENCESCONFIGSTORE_H

#include <Preferences.h>

#include "ConfigService.h"

class PreferencesConfigStore : public ConfigStore {
 public:
  bool load(DeviceConfig* output) override;
  bool save(const DeviceConfig& config) override;
  bool clear() override;

 private:
  struct Slot {
    bool valid = false;
    uint64_t generation = 0;
    DeviceConfig config;
  };

  static constexpr const char* NAMESPACE = "glowcfg";
  static constexpr const char* SLOT_A = "slotA";
  static constexpr const char* SLOT_B = "slotB";
  static uint32_t crc32(const String& value);
  static Slot decode(const String& stored);
  static String checksumInput(const String& payload, uint64_t generation);
  static String encode(const DeviceConfig& config, uint64_t generation);
};

#endif
