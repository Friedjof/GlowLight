#include "NetworkService.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>

uint32_t NetworkService::backoffFor(uint8_t attempt) {
  uint32_t delayMs = BACKOFF_MIN_MS;
  for (uint8_t i = 1; i < attempt && delayMs < BACKOFF_MAX_MS; ++i) delayMs *= 2;
  return delayMs > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : delayMs;
}

void NetworkService::setup() { this->setup(NetworkConfig()); }

void NetworkService::setup(const NetworkConfig& configuration) {
  this->stopResponders();
  this->config = configuration;
  if (this->config.fallbackChannel < 1 || this->config.fallbackChannel > 13) {
    this->config.fallbackChannel = 1;
  }
  if (this->config.hostname.isEmpty()) this->config.hostname = GLOW_HOSTNAME;
  this->attempts = 0;
  this->lastChannel = 0;
  this->lastOnlineChannel = 0;
  this->fallbackParked = false;
  this->fallbackParkFailed = false;
  this->channelReadFailed = false;
  this->retryDelayMs = BACKOFF_MIN_MS;

  // Station mode is required either way: ESP-NOW needs an initialised
  // interface even when this lamp never joins a network.
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  // Modem sleep would make the radio miss ESP-NOW frames between beacons.
  WiFi.setSleep(false);

  if (!this->config.enabled || this->config.ssid.isEmpty()) {
    WiFi.disconnect(false, false);
    this->parkOnFallbackChannel();
    this->enterState(State::Inactive);
    Serial.printf("[INFO] WiFi disabled, ESP-NOW parked on channel %u\n",
                  this->config.fallbackChannel);
    return;
  }

  WiFi.setHostname(this->config.hostname.c_str());
  WiFi.setAutoReconnect(false);  // the state machine below owns reconnects
  this->beginConnect();
}

bool NetworkService::setupProvisioning(const NetworkConfig& configuration,
                                       const String& apSsid,
                                       const String& apPassword) {
  this->stopResponders();
  this->config = configuration;
  if (this->config.fallbackChannel < 1 || this->config.fallbackChannel > 13) {
    this->config.fallbackChannel = 1;
  }
  if (this->config.hostname.isEmpty()) this->config.hostname = GLOW_HOSTNAME;
  if (apSsid.isEmpty() || apSsid.length() > 32 || apPassword.length() < 8 ||
      apPassword.length() > 63) {
    Serial.println("[ERROR] Provisioning AP configuration is invalid");
    return false;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setHostname(this->config.hostname.c_str());
  WiFi.disconnect(false, false);
  if (!WiFi.softAP(apSsid.c_str(), apPassword.c_str(),
                   this->config.fallbackChannel)) {
    Serial.println("[ERROR] Provisioning AP could not be started");
    return false;
  }

  this->enterState(State::Provisioning);
  this->publishChannel();
  Serial.printf("[INFO] Provisioning AP '%s' available at %s on channel %u\n",
                apSsid.c_str(), WiFi.softAPIP().toString().c_str(),
                this->config.fallbackChannel);
  return true;
}

void NetworkService::beginConnect() {
  this->fallbackParked = false;
  if (this->attempts < 255) ++this->attempts;
  if (this->lastOnlineChannel != 0) {
    Serial.printf("[INFO] WiFi reconnecting to '%s' on channel %u (attempt %u)\n",
                  this->config.ssid.c_str(), this->lastOnlineChannel, this->attempts);
    WiFi.begin(this->config.ssid.c_str(), this->config.password.c_str(),
               this->lastOnlineChannel);
  } else {
    Serial.printf("[INFO] WiFi connecting to '%s' (attempt %u)\n",
                  this->config.ssid.c_str(), this->attempts);
    WiFi.begin(this->config.ssid.c_str(), this->config.password.c_str());
  }
  this->enterState(State::Connecting);
}

void NetworkService::enterState(State next) {
  this->currentState = next;
  this->stateEnteredAt = millis();
}

void NetworkService::scheduleRetry(const char* reason) {
  Serial.println(reason);
  WiFi.disconnect(false, false);
  this->stopResponders();
  this->parkOnFallbackChannel();

  uint8_t attempt = this->attempts == 0 ? 1 : this->attempts;
  uint32_t baseDelay = backoffFor(attempt);
  uint32_t jitterWindow = baseDelay / 4;
  uint32_t jitter = jitterWindow == 0 ? 0 : esp_random() % (jitterWindow + 1);
  this->retryDelayMs = baseDelay - jitter;
  this->enterState(State::Retrying);
}

void NetworkService::parkOnFallbackChannel() {
  // Every lamp without a connection parks on the same channel, so a group still
  // finds itself when the access point is unavailable.
  this->lastFallbackAttemptAt = millis();
  esp_err_t result =
      esp_wifi_set_channel(this->config.fallbackChannel, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    this->fallbackParked = false;
    if (!this->fallbackParkFailed) {
      Serial.printf("[WARN] Could not set fallback channel %u: %d\n",
                    this->config.fallbackChannel, static_cast<int>(result));
    }
    this->fallbackParkFailed = true;
  } else {
    uint8_t current = 0;
    this->fallbackParked = this->readRadioChannel(&current) &&
                           current == this->config.fallbackChannel;
    if (this->fallbackParkFailed && this->fallbackParked) {
      Serial.printf("[INFO] ESP-NOW parked on fallback channel %u\n",
                    this->config.fallbackChannel);
    }
    this->fallbackParkFailed = false;
  }
  this->publishChannel();
}

bool NetworkService::readRadioChannel(uint8_t* channel) const {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK || primary == 0) return false;
  *channel = primary;
  return true;
}

void NetworkService::publishChannel() {
  uint8_t current = 0;
  if (!this->readRadioChannel(&current)) {
    if (!this->channelReadFailed) Serial.println("[WARN] Could not read WiFi radio channel");
    this->channelReadFailed = true;
    return;
  }
  this->channelReadFailed = false;
  if (current == this->lastChannel) return;

  this->lastChannel = current;
  if (this->channelCallback != nullptr) this->channelCallback(current);
}

void NetworkService::startResponders() {
  if (this->respondersStarted) return;

  if (MDNS.begin(this->config.hostname.c_str())) {
    Serial.printf("[INFO] Reachable as %s.local\n", this->config.hostname.c_str());
    this->respondersStarted = true;
  } else {
    Serial.println("[WARN] mDNS could not be started");
  }
}

void NetworkService::stopResponders() {
  if (!this->respondersStarted) return;
  MDNS.end();
  this->respondersStarted = false;
}

void NetworkService::loop() {
  uint32_t now = millis();
  if (this->currentState == State::Provisioning) {
    this->publishChannel();
    return;
  }
  if (this->currentState == State::Inactive) {
    if (!this->fallbackParked && now - this->lastFallbackAttemptAt >= FALLBACK_RETRY_MS) {
      this->parkOnFallbackChannel();
    }
    return;
  }

  bool connected = WiFi.status() == WL_CONNECTED;

  switch (this->currentState) {
    case State::Connecting:
      if (connected) {
        uint8_t currentChannel = 0;
        if (this->readRadioChannel(&currentChannel)) {
          this->lastOnlineChannel = currentChannel;
        }
        this->attempts = 0;
        this->enterState(State::Online);
        this->publishChannel();
        this->startResponders();
        Serial.printf("[INFO] WiFi connected, %s on channel %u\n",
                      this->address().c_str(), this->channel());
      } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
        this->scheduleRetry("[WARN] WiFi network not found");
      } else if (WiFi.status() == WL_CONNECT_FAILED) {
        this->scheduleRetry("[WARN] WiFi authentication failed");
      } else {
        uint32_t timeout = this->lastOnlineChannel == 0 ? CONNECT_TIMEOUT_MS
                                                        : RECONNECT_TIMEOUT_MS;
        if (now - this->stateEnteredAt >= timeout) {
          this->scheduleRetry("[WARN] WiFi connection timed out");
        }
      }
      break;

    case State::Online: {
      if (!connected) {
        this->scheduleRetry("[WARN] WiFi connection lost");
        break;
      }
      // A roam or a channel switch on the access point moves ESP-NOW with us.
      uint8_t currentChannel = 0;
      if (this->readRadioChannel(&currentChannel)) {
        this->lastOnlineChannel = currentChannel;
      }
      this->publishChannel();
      break;
    }

    case State::Retrying:
      if (!this->fallbackParked &&
          now - this->lastFallbackAttemptAt >= FALLBACK_RETRY_MS) {
        this->parkOnFallbackChannel();
      }
      if (now - this->stateEnteredAt >= this->retryDelayMs) this->beginConnect();
      break;

    case State::Inactive:
    case State::Provisioning:
      break;
  }
}

bool NetworkService::isConnected() const { return this->currentState == State::Online; }

uint8_t NetworkService::channel() const {
  uint8_t current = 0;
  return this->readRadioChannel(&current) ? current : this->config.fallbackChannel;
}

String NetworkService::address() const {
  if (this->isProvisioning()) return WiFi.softAPIP().toString();
  return this->isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
}

void NetworkService::onChannelChanged(std::function<void(uint8_t)> callback) {
  this->channelCallback = callback;
}
