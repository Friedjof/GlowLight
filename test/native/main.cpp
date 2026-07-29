#include <cstdio>
#include <cstring>

#include "Arduino.h"
#include "support.h"

int main(int argc, char** argv) {
  const char* filter = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--verbose") == 0) {
      glow_shim::serialEcho = true;
    } else {
      filter = argv[i];
    }
  }

  printf("GlowLight secure transport — native tests\n\n");
  return glowtest::runAllTests(filter);
}
