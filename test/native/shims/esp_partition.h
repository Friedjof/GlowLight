#ifndef GLOW_SHIM_ESP_PARTITION_H
#define GLOW_SHIM_ESP_PARTITION_H

#include <cstdint>

struct esp_partition_t {
  uint32_t address;
  uint32_t size;
};

#endif
