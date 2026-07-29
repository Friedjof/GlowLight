#include "OtaService.h"
#include "Update.h"
#include "WebServer.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "support.h"

using namespace glowtest;

struct GlowOtaTestAccess {
  static void showPage(OtaService& service) { service.handlePage(); }
  static String token(const OtaService& service) { return service.uploadToken; }
  static void upload(OtaService& service, HTTPUploadStatus status,
                      const String& token = String()) {
    static uint8_t bytes[16] = {0xe9};
    service.server->headers["Content-Type"] =
        "multipart/form-data; boundary=test";
    service.server->arguments["token"] = token.c_str();
    service.server->currentUpload.status = status;
    service.server->currentUpload.buf = bytes;
    service.server->currentUpload.currentSize = sizeof(bytes);
    service.handleUpload();
  }
  static void complete(OtaService& service) { service.handleUploadComplete(); }
  static void rawUpload(OtaService& service) {
    service.server->headers.erase("Content-Type");
    service.handleUpload();
  }
  static bool succeeded(const OtaService& service) {
    return service.uploadSucceeded;
  }
  static bool recovering(const OtaService& service) {
    return service.bootRecoveryRequired;
  }
};

namespace {
NetworkConfig onlineConfig() {
  NetworkConfig config;
  config.enabled = true;
  config.ssid = "testnet";
  config.password = "secret123";
  config.hostname = "glow-ota";
  config.fallbackChannel = 6;
  return config;
}

void resetOtaShims() {
  glow_shim::clockMillis = 0;
  glow_shim::resetNetwork();
  glow_shim::resetWebServer();
  glow_shim::resetUpdate();
  glow_shim::restartCalls = 0;
  glow_shim::imageVerifyResult = ESP_OK;
  glow_shim::imageVerifyCalls = 0;
  glow_shim::updatePartitionAvailable = true;
  glow_shim::restoreBootPartitionCalls = 0;
  glow_shim::restoreBootPartitionResult = ESP_OK;
}

void connect(NetworkService& network) {
  glow_shim::radioChannel = 6;
  glow_shim::wifiStatus = WL_CONNECTED;
  network.loop();
}

OtaConfig enabledOta() {
  OtaConfig config;
  config.enabled = true;
  config.password = "unique-ota-password";
  return config;
}
}  // namespace

GLOW_TEST(ota_server_follows_the_infrastructure_wifi_lifecycle) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  OtaService ota;
  CHECK(ota.setup(network, enabledOta()));
  ota.loop();
  CHECK(!ota.active());

  connect(network);
  ota.loop();
  CHECK(ota.active());
  CHECK_EQ(glow_shim::webBeginCalls, 1);

  glow_shim::wifiStatus = WL_DISCONNECTED;
  network.loop();
  ota.loop();
  CHECK(!ota.active());
  CHECK_EQ(glow_shim::webStopCalls, 1);
}

GLOW_TEST(unauthenticated_or_tokenless_upload_never_starts_flash_writes) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();

  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  String token = GlowOtaTestAccess::token(ota);
  glow_shim::webAuthenticated = false;
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  CHECK_EQ(glow_shim::updateBeginCalls, 0);

  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, "wrong-token");
  CHECK_EQ(glow_shim::updateBeginCalls, 0);
}

GLOW_TEST(a_bodyless_digest_probe_never_accesses_the_upload_object) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();

  GlowOtaTestAccess::rawUpload(ota);
  CHECK_EQ(glow_shim::updateBeginCalls, 0);
  CHECK_EQ(glow_shim::imageVerifyCalls, 0);
  CHECK_EQ(glow_shim::restartCalls, 0);
}

GLOW_TEST(truncated_or_invalid_ota_image_restores_the_running_partition) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();
  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  String token = GlowOtaTestAccess::token(ota);

  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_WRITE);
  glow_shim::imageVerifyResult = ESP_FAIL;
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_END);
  CHECK(!GlowOtaTestAccess::succeeded(ota));
  CHECK_EQ(glow_shim::imageVerifyCalls, 1);
  CHECK_EQ(glow_shim::restoreBootPartitionCalls, 1);

  GlowOtaTestAccess::complete(ota);
  CHECK_EQ(glow_shim::webLastStatus, 400);
  CHECK_EQ(glow_shim::restartCalls, 0);
}

GLOW_TEST(valid_ota_image_restarts_only_after_the_success_response) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();
  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  String token = GlowOtaTestAccess::token(ota);

  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_WRITE);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_END);
  CHECK(GlowOtaTestAccess::succeeded(ota));
  GlowOtaTestAccess::complete(ota);
  CHECK_EQ(glow_shim::webLastStatus, 200);
  CHECK_EQ(glow_shim::restartCalls, 0);

  glow_shim::clockMillis += 1000;
  ota.loop();
  CHECK_EQ(glow_shim::restartCalls, 1);
}

GLOW_TEST(a_second_file_part_cancels_an_already_selected_ota_image) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();
  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  String token = GlowOtaTestAccess::token(ota);

  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_WRITE);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_END);
  CHECK(GlowOtaTestAccess::succeeded(ota));

  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  CHECK(!GlowOtaTestAccess::succeeded(ota));
  CHECK_EQ(glow_shim::restoreBootPartitionCalls, 1);
  GlowOtaTestAccess::complete(ota);
  CHECK_EQ(glow_shim::webLastStatus, 400);
}

GLOW_TEST(failed_boot_target_recovery_is_retried_before_serving_requests) {
  resetOtaShims();
  NetworkService network;
  network.setup(onlineConfig());
  connect(network);
  OtaService ota;
  ota.setup(network, enabledOta());
  ota.loop();
  glow_shim::webAuthenticated = true;
  GlowOtaTestAccess::showPage(ota);
  String token = GlowOtaTestAccess::token(ota);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_START, token);
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_WRITE);

  glow_shim::imageVerifyResult = ESP_FAIL;
  glow_shim::restoreBootPartitionResult = ESP_FAIL;
  GlowOtaTestAccess::upload(ota, UPLOAD_FILE_END);
  CHECK(GlowOtaTestAccess::recovering(ota));
  GlowOtaTestAccess::complete(ota);
  CHECK_EQ(glow_shim::webLastStatus, 500);

  glow_shim::restoreBootPartitionResult = ESP_OK;
  ota.loop();
  CHECK(!GlowOtaTestAccess::recovering(ota));
  CHECK_EQ(glow_shim::restoreBootPartitionCalls, 2);
}
