/*
 * NetworkService.h
 *
 * Owns the WiFi radio. ESP-NOW and WiFi share one transceiver and therefore one
 * channel: while a lamp is associated with an access point, the access point
 * dictates the channel. All lamps of a group must stay on the same current
 * channel; using one access point is the simplest way to guarantee that.
 *
 * Without a connection the service parks the radio on the configured fallback
 * channel, so that lamps still find each other when the access point is gone.
 *
 * Connecting never blocks; the loop drives a small state machine so the LED
 * animation keeps running.
 */

#ifndef NETWORKSERVICE_H
#define NETWORKSERVICE_H

#include <Arduino.h>
#include <functional>

#include "GlowConfig.h"

#ifndef WIFI_ON
#define WIFI_ON false
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef GLOW_HOSTNAME
#define GLOW_HOSTNAME "glowlight"
#endif

#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif

// Runtime configuration. Defaults come from GlowConfig.h; phase 4 will let
// stored values from NVS override them without touching this service.
struct NetworkConfig {
  bool enabled = WIFI_ON;
  String ssid = WIFI_SSID;
  String password = WIFI_PASSWORD;
  String hostname = GLOW_HOSTNAME;
  uint8_t fallbackChannel = ESPNOW_CHANNEL;
};

class NetworkService {
 public:
  // CamelCase on purpose: the Arduino core defines DISABLED, CHANGE and friends
  // as macros, which would be substituted inside an all-caps enumeration.
  enum class State : uint8_t {
    Inactive,    // no credentials, or switched off in the configuration
    Connecting,  // association in progress
    Online,      // associated, channel follows the access point
    Retrying,    // attempt failed, backing off before the next try
    Provisioning, // local setup AP on the ESP-NOW fallback channel
  };

  void setup();
  void setup(const NetworkConfig& config);
  bool setupProvisioning(const NetworkConfig& config, const String& apSsid,
                         const String& apPassword);
  void loop();

  bool isConnected() const;
  bool isProvisioning() const { return this->currentState == State::Provisioning; }
  State state() const { return this->currentState; }
  uint8_t channel() const;
  uint8_t fallbackChannel() const { return this->config.fallbackChannel; }
  const String& hostname() const { return this->config.hostname; }
  String address() const;

  // Fired whenever the radio channel changes, so ESP-NOW peers can be refreshed.
  void onChannelChanged(std::function<void(uint8_t)> callback);

  // Test seam: the retry schedule in milliseconds for a given attempt count.
  static uint32_t backoffFor(uint8_t attempt);

  static constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
  static constexpr uint32_t RECONNECT_TIMEOUT_MS = 3000;
  static constexpr uint32_t BACKOFF_MIN_MS = 10000;
  static constexpr uint32_t BACKOFF_MAX_MS = 60000;
  static constexpr uint32_t FALLBACK_RETRY_MS = 250;

 private:
  void enterState(State next);
  void beginConnect();
  void scheduleRetry(const char* reason);
  void parkOnFallbackChannel();
  bool readRadioChannel(uint8_t* channel) const;
  void publishChannel();
  void startResponders();
  void stopResponders();

  NetworkConfig config;
  State currentState = State::Inactive;
  uint32_t stateEnteredAt = 0;
  uint32_t retryDelayMs = BACKOFF_MIN_MS;
  uint32_t lastFallbackAttemptAt = 0;
  uint8_t attempts = 0;
  uint8_t lastChannel = 0;
  uint8_t lastOnlineChannel = 0;
  bool fallbackParked = false;
  bool fallbackParkFailed = false;
  bool channelReadFailed = false;
  bool respondersStarted = false;
  std::function<void(uint8_t)> channelCallback = nullptr;
};

#endif
