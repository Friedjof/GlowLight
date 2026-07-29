#ifndef GLOW_SHIM_UPDATE_H
#define GLOW_SHIM_UPDATE_H

#include <cstddef>
#include <cstdint>

#define UPDATE_SIZE_UNKNOWN static_cast<size_t>(-1)
#define U_FLASH 0

namespace glow_shim {
inline bool updateBeginResult = true;
inline bool updateEndResult = true;
inline bool updateWriteResult = true;
inline int updateBeginCalls = 0;
inline int updateEndCalls = 0;
inline int updateAbortCalls = 0;
inline size_t updateBytesWritten = 0;

inline void resetUpdate() {
  updateBeginResult = true;
  updateEndResult = true;
  updateWriteResult = true;
  updateBeginCalls = 0;
  updateEndCalls = 0;
  updateAbortCalls = 0;
  updateBytesWritten = 0;
}
}  // namespace glow_shim

class UpdateClass {
 public:
  bool begin(size_t, int) {
    ++glow_shim::updateBeginCalls;
    return glow_shim::updateBeginResult;
  }
  size_t write(uint8_t*, size_t length) {
    if (!glow_shim::updateWriteResult) return 0;
    glow_shim::updateBytesWritten += length;
    return length;
  }
  bool end(bool) {
    ++glow_shim::updateEndCalls;
    return glow_shim::updateEndResult;
  }
  void abort() { ++glow_shim::updateAbortCalls; }
};

inline UpdateClass Update;

#endif
