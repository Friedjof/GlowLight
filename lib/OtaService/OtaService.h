#ifndef OTASERVICE_H
#define OTASERVICE_H

#include <Arduino.h>
#include <WebServer.h>
#include <esp_partition.h>

#include "NetworkService.h"

#ifdef GLOW_UNIT_TEST
struct GlowOtaTestAccess;
#endif

struct OtaConfig {
  bool enabled = false;
  String password;
};

class OtaService {
#ifdef GLOW_UNIT_TEST
  friend struct GlowOtaTestAccess;
#endif
 public:
  ~OtaService();
  bool setup(NetworkService& network, const OtaConfig& config);
  void loop();
  bool active() const { return this->serverStarted; }

 private:
  static constexpr const char* USERNAME = "glowlight";
  static constexpr const char* REALM = "GlowLight OTA";
  static constexpr uint32_t RESTART_DELAY_MS = 1000;

  NetworkService* network = nullptr;
  WebServer* server = nullptr;
  String password;
  String uploadToken;
  const esp_partition_t* targetPartition = nullptr;
  bool configured = false;
  bool serverStarted = false;
  bool uploadAuthorized = false;
  bool updateStarted = false;
  bool uploadSucceeded = false;
  bool uploadFailed = false;
  bool bootRecoveryRequired = false;
  bool restartPending = false;
  uint32_t restartAt = 0;

  bool authenticate();
  void requestAuthentication();
  void startServer();
  void stopServer();
  void handlePage();
  void handleUpload();
  void handleUploadComplete();
  void resetUploadState();
  bool restoreRunningBootPartition();
  static String makeToken();
};

#endif
