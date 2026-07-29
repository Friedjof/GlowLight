// Host shim for the mDNS responder. Records the announced hostname.
#ifndef GLOW_SHIM_ESPMDNS_H
#define GLOW_SHIM_ESPMDNS_H

#include <string>

namespace glow_shim {
extern std::string mdnsHostname;
extern int mdnsBeginCalls;
extern int mdnsEndCalls;
extern bool mdnsShouldFail;
}  // namespace glow_shim

class MDNSStub {
 public:
  bool begin(const char* hostname) {
    ++glow_shim::mdnsBeginCalls;
    if (glow_shim::mdnsShouldFail) return false;
    glow_shim::mdnsHostname = hostname != nullptr ? hostname : "";
    return true;
  }
  void end() {
    ++glow_shim::mdnsEndCalls;
    glow_shim::mdnsHostname.clear();
  }
};

extern MDNSStub MDNS;

#endif
