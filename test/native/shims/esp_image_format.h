#ifndef GLOW_SHIM_ESP_IMAGE_FORMAT_H
#define GLOW_SHIM_ESP_IMAGE_FORMAT_H

#include <cstdint>

#include "esp_now.h"

enum esp_image_load_mode_t { ESP_IMAGE_VERIFY };
struct esp_partition_pos_t {
  uint32_t offset;
  uint32_t size;
};
struct esp_image_metadata_t {};

namespace glow_shim {
inline esp_err_t imageVerifyResult = ESP_OK;
inline int imageVerifyCalls = 0;
}

inline esp_err_t esp_image_verify(esp_image_load_mode_t,
                                  const esp_partition_pos_t*,
                                  esp_image_metadata_t*) {
  ++glow_shim::imageVerifyCalls;
  return glow_shim::imageVerifyResult;
}

#endif
