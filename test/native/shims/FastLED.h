#ifndef GLOW_SHIM_FASTLED_H
#define GLOW_SHIM_FASTLED_H

#include <cstdint>

struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  CRGB(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
      : r(red), g(green), b(blue) {}

  bool operator==(const CRGB& other) const {
    return r == other.r && g == other.g && b == other.b;
  }
};

#endif
