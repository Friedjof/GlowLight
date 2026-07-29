// Host shim for esp_system.h. The RNG is a seedable xorshift so that boot IDs
// and challenges are reproducible across test runs.
#ifndef GLOW_SHIM_ESP_SYSTEM_H
#define GLOW_SHIM_ESP_SYSTEM_H

#include <cstddef>
#include <cstdint>

namespace glow_shim {
void seedRandom(uint64_t seed);
}  // namespace glow_shim

void esp_fill_random(void* buffer, size_t length);
uint32_t esp_random(void);

#endif
