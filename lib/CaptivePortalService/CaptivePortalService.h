#ifndef CAPTIVEPORTALSERVICE_H
#define CAPTIVEPORTALSERVICE_H

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "ConfigService.h"
#include "NetworkService.h"

class CaptivePortalService {
 public:
  bool setup(ConfigStore& store, NetworkService& network,
             const DeviceConfig& current);
  void loop();
  bool active() const { return this->started; }

 private:
  static constexpr size_t MAX_REQUEST_SIZE = 2048;
  static constexpr uint32_t RESTART_DELAY_MS = 500;
  static constexpr uint32_t PORTAL_TIMEOUT_MS = 10 * 60 * 1000;

  ConfigStore* store = nullptr;
  NetworkService* network = nullptr;
  DeviceConfig current;
  DNSServer dns;
  WebServer server{80};
  String sessionToken;
  bool started = false;
  bool restartPending = false;
  uint32_t restartAt = 0;
  uint32_t startedAt = 0;

  bool authorized();
  void addNoStoreHeaders();
  void sendJson(int status, const JsonDocument& document);
  void sendError(int status, const char* code, const String& message);
  void handleRoot();
  void handleGetConfig();
  void handleSaveConfig();
  void handleFactoryReset();
  void scheduleRestart();
};

#endif
