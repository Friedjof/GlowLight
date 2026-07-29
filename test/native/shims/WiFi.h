// Host shim for the Arduino WiFi class.
#ifndef GLOW_SHIM_WIFI_H
#define GLOW_SHIM_WIFI_H

#include <cstdint>
#include <cstring>
#include <string>

#include "Arduino.h"

#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3

#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4
#define WL_DISCONNECTED 6

namespace glow_shim {
// MAC address handed out by WiFi.macAddress(); tests may change it.
extern uint8_t localMac[6];
// Radio state the tests drive.
extern int wifiStatus;
extern bool wifiSleep;
extern int wifiMode;
extern int wifiBeginCalls;
extern int wifiBeginChannel;
extern int wifiDisconnectCalls;
extern std::string wifiSsid;
extern std::string wifiPassword;
extern std::string wifiHostname;
extern int wifiSoftApCalls;
extern int wifiSoftApChannel;
extern bool wifiSoftApShouldFail;
extern std::string wifiSoftApSsid;
extern std::string wifiSoftApPassword;
extern uint8_t radioChannel;
}  // namespace glow_shim

class IPAddressStub {
 public:
  String toString() const { return String("192.168.1.42"); }
};

class WiFiClass {
 public:
  void mode(int value) { glow_shim::wifiMode = value; }
  void persistent(bool) {}
  void setSleep(bool enabled) { glow_shim::wifiSleep = enabled; }
  void setAutoReconnect(bool) {}
  void setHostname(const char* name) { glow_shim::wifiHostname = name ? name : ""; }
  void macAddress(uint8_t* mac) { memcpy(mac, glow_shim::localMac, 6); }
  int status() const { return glow_shim::wifiStatus; }
  IPAddressStub localIP() const { return IPAddressStub(); }
  IPAddressStub softAPIP() const { return IPAddressStub(); }

  bool softAP(const char* ssid, const char* password, int channel = 1) {
    ++glow_shim::wifiSoftApCalls;
    glow_shim::wifiSoftApSsid = ssid ? ssid : "";
    glow_shim::wifiSoftApPassword = password ? password : "";
    glow_shim::wifiSoftApChannel = channel;
    if (!glow_shim::wifiSoftApShouldFail) glow_shim::radioChannel = channel;
    return !glow_shim::wifiSoftApShouldFail;
  }

  void begin(const char* ssid, const char* password, int32_t channel = 0,
             const uint8_t* = nullptr, bool = true) {
    ++glow_shim::wifiBeginCalls;
    glow_shim::wifiBeginChannel = channel;
    glow_shim::wifiSsid = ssid ? ssid : "";
    glow_shim::wifiPassword = password ? password : "";
    glow_shim::wifiStatus = WL_DISCONNECTED;
  }

  void disconnect() { this->disconnect(false, false); }
  void disconnect(bool, bool) {
    ++glow_shim::wifiDisconnectCalls;
    glow_shim::wifiStatus = WL_DISCONNECTED;
  }
};

extern WiFiClass WiFi;

#endif
