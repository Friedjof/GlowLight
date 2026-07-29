#include "OtaService.h"

#include <Update.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace {
const char OTA_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GlowLight update</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#0d1014;color:#edf4f8}body{margin:0;padding:2rem 1rem}main{max-width:34rem;margin:auto}h1{color:#7de2d1}form{padding:1.25rem;border:1px solid #34414a;border-radius:.7rem;background:#151a20}input{display:block;width:100%;box-sizing:border-box;margin:1rem 0;padding:.8rem;border:1px solid #51616c;border-radius:.4rem;background:#0d1014;color:inherit}button{padding:.8rem 1rem;border:0;border-radius:.4rem;background:#7de2d1;color:#06211d;font-weight:700}.hint{color:#a9b3ba}</style></head>
<body><main><h1>GlowLight firmware update</h1><p>Select a PlatformIO firmware image for this exact board.</p><form method="POST" action="/api/ota" enctype="multipart/form-data"><input type="hidden" name="token" value="{{TOKEN}}"><input type="file" name="firmware" accept=".bin,application/octet-stream" required><button type="submit">Install and restart</button></form><p class="hint">Do not disconnect power or WiFi during the upload.</p></main></body></html>)HTML";
}

OtaService::~OtaService() { this->stopServer(); }

bool OtaService::setup(NetworkService& networkService, const OtaConfig& config) {
  this->network = &networkService;
  this->configured = config.enabled && config.password.length() >= 12 &&
                     config.password.length() <= 63;
  if (!this->configured) return !config.enabled;
  this->password = config.password;

  return true;
}

void OtaService::loop() {
  if (!this->configured || this->network == nullptr) return;
  if (this->bootRecoveryRequired && !this->restoreRunningBootPartition()) return;
  if (this->restartPending && millis() - this->restartAt >= RESTART_DELAY_MS) {
    ESP.restart();
    return;
  }

  if (this->network->isConnected()) {
    if (!this->serverStarted) this->startServer();
    if (this->server != nullptr) this->server->handleClient();
  } else if (this->serverStarted) {
    this->stopServer();
  }
}

bool OtaService::authenticate() {
  return this->server != nullptr &&
         this->server->authenticate(USERNAME, this->password.c_str());
}

void OtaService::requestAuthentication() {
  if (this->server != nullptr) {
    this->server->requestAuthentication(DIGEST_AUTH, REALM);
  }
}

void OtaService::startServer() {
  this->server = new WebServer(80);
  if (this->server == nullptr) {
    Serial.println("[ERROR] OTA server allocation failed");
    return;
  }
  const char* requestedHeaders[] = {"Content-Type"};
  this->server->collectHeaders(requestedHeaders, 1);
  this->server->on("/update", HTTP_GET, [this]() { this->handlePage(); });
  this->server->on(
      "/api/ota", HTTP_POST,
      [this]() { this->handleUploadComplete(); },
      [this]() { this->handleUpload(); });
  this->server->begin();
  this->serverStarted = true;
  Serial.println("[INFO] Password-protected OTA available at /update");
}

void OtaService::stopServer() {
  if (this->updateStarted) Update.abort();
  this->resetUploadState();
  this->uploadToken = "";
  if (this->server != nullptr) {
    this->server->stop();
    delete this->server;
    this->server = nullptr;
  }
  this->serverStarted = false;
}

void OtaService::handlePage() {
  if (!this->authenticate()) {
    this->requestAuthentication();
    return;
  }
  this->uploadToken = makeToken();
  String page = FPSTR(OTA_HTML);
  page.replace("{{TOKEN}}", this->uploadToken);
  this->server->sendHeader("Cache-Control", "no-store");
  this->server->sendHeader("X-Content-Type-Options", "nosniff");
  this->server->send(200, "text/html", page);
}

void OtaService::handleUpload() {
  // WebServer routes the bodyless Digest probe through the raw callback. Its
  // upload() accessor dereferences a null pointer until multipart parsing starts.
  if (this->server == nullptr || !this->server->hasHeader("Content-Type") ||
      !this->server->header("Content-Type").startsWith("multipart/")) return;

  HTTPUpload& upload = this->server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (this->updateStarted || this->uploadSucceeded) {
      if (this->updateStarted) Update.abort();
      if (this->uploadSucceeded) this->restoreRunningBootPartition();
      this->resetUploadState();
      this->uploadToken = "";
      this->uploadFailed = true;
      return;
    }
    this->resetUploadState();
    bool validToken = !this->uploadToken.isEmpty() &&
                      this->server->arg("token") == this->uploadToken;
    this->uploadToken = "";
    this->uploadAuthorized = this->authenticate() && validToken;
    if (!this->uploadAuthorized) return;
    this->targetPartition = esp_ota_get_next_update_partition(nullptr);
    if (this->targetPartition == nullptr) {
      this->uploadFailed = true;
      return;
    }
    this->updateStarted = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
    this->uploadFailed = !this->updateStarted;
    if (this->uploadFailed) Serial.println("[ERROR] OTA update partition is unavailable");
    return;
  }

  if (!this->uploadAuthorized || !this->updateStarted || this->uploadFailed) return;
  if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      this->uploadFailed = true;
      Update.abort();
      this->updateStarted = false;
      Serial.println("[ERROR] OTA firmware write failed");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    bool ended = Update.end(true);
    esp_image_metadata_t metadata = {};
    esp_partition_pos_t targetPosition = {
        this->targetPartition == nullptr ? 0 : this->targetPartition->address,
        this->targetPartition == nullptr ? 0 : this->targetPartition->size};
    bool verified = ended && this->targetPartition != nullptr &&
                    esp_image_verify(ESP_IMAGE_VERIFY, &targetPosition,
                                     &metadata) == ESP_OK;
    if (ended && !verified) {
      this->restoreRunningBootPartition();
    }
    this->uploadSucceeded = ended && verified;
    this->uploadFailed = !this->uploadSucceeded;
    this->updateStarted = false;
    if (!this->uploadSucceeded) Serial.println("[ERROR] OTA image validation failed");
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    this->updateStarted = false;
    this->uploadFailed = true;
    Serial.println("[WARN] OTA upload aborted");
  }
}

void OtaService::handleUploadComplete() {
  if (!this->authenticate()) {
    if (this->updateStarted) Update.abort();
    this->resetUploadState();
    this->requestAuthentication();
    return;
  }
  this->server->sendHeader("Cache-Control", "no-store");
  this->server->sendHeader("Connection", "close");
  if (this->bootRecoveryRequired) {
    this->server->send(500, "application/json",
                       "{\"ok\":false,\"error\":\"BOOT_RECOVERY_PENDING\"}");
    this->resetUploadState();
    return;
  }
  if (!this->uploadSucceeded || this->uploadFailed) {
    this->server->send(400, "application/json",
                       "{\"ok\":false,\"error\":\"INVALID_FIRMWARE\"}");
    this->resetUploadState();
    return;
  }

  this->server->send(200, "application/json",
                     "{\"ok\":true,\"restart\":true}");
  this->restartPending = true;
  this->restartAt = millis();
}

void OtaService::resetUploadState() {
  this->uploadAuthorized = false;
  this->updateStarted = false;
  this->uploadSucceeded = false;
  this->uploadFailed = false;
  this->targetPartition = nullptr;
}

String OtaService::makeToken() {
  char token[33];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(token + i * 8, sizeof(token) - i * 8, "%08lx",
             static_cast<unsigned long>(esp_random()));
  }
  token[32] = '\0';
  return String(token);
}

bool OtaService::restoreRunningBootPartition() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  bool restored = running != nullptr &&
                  esp_ota_set_boot_partition(running) == ESP_OK;
  this->bootRecoveryRequired = !restored;
  if (!restored) Serial.println("[ERROR] OTA boot target recovery failed, retrying");
  return restored;
}
