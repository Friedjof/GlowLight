#ifndef GLOW_SHIM_ESP_OTA_OPS_H
#define GLOW_SHIM_ESP_OTA_OPS_H

#include "esp_now.h"
#include "esp_partition.h"

namespace glow_shim {
inline esp_partition_t runningPartition{0x10000, 0x140000};
inline esp_partition_t updatePartition{0x150000, 0x140000};
inline bool updatePartitionAvailable = true;
inline int restoreBootPartitionCalls = 0;
inline esp_err_t restoreBootPartitionResult = ESP_OK;
}

inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) {
  return glow_shim::updatePartitionAvailable ? &glow_shim::updatePartition : nullptr;
}
inline const esp_partition_t* esp_ota_get_running_partition() {
  return &glow_shim::runningPartition;
}
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t*) {
  ++glow_shim::restoreBootPartitionCalls;
  return glow_shim::restoreBootPartitionResult;
}

#endif
