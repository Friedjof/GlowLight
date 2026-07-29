/*
 * HomeAssistantService.h
 *
 * MQTT adapter in front of the control API. It owns the broker connection and
 * the publish schedule; the actual translation lives in HomeAssistantProtocol,
 * which is pure and host-tested.
 *
 * The service never interprets a mode. It asks the Controller for its
 * capabilities and its state and republishes whatever it finds there.
 */

#ifndef HOMEASSISTANTSERVICE_H
#define HOMEASSISTANTSERVICE_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "Controller.h"
#include "HomeAssistantProtocol.h"
#include "NetworkService.h"

struct HomeAssistantConfig {
  bool enabled = false;
  String host;
  uint16_t port = 1883;
  String user;
  String password;
  String discoveryPrefix = "homeassistant";
  String baseTopicPrefix = "glowlight";
  // "group" lets a command from Home Assistant move the whole lamp group; the
  // service falls back to "local" whenever group publishing is not possible.
  String commandScope = "group";
};

class HomeAssistantService {
 public:
  bool setup(NetworkService& network, Controller& controller,
             const HomeAssistantConfig& config, const String& deviceId,
             const String& firmwareVersion);
  void loop();
  bool connected() const;

  static constexpr uint32_t RECONNECT_MIN_MS = 5000;
  static constexpr uint32_t RECONNECT_MAX_MS = 60000;
  static constexpr uint32_t STATE_POLL_MS = 250;
  // Discovery documents carry the whole device block and are the largest thing
  // this service publishes.
  static constexpr uint16_t MQTT_BUFFER_SIZE = 1536;
  // PubSubClient connects synchronously; a short socket timeout keeps the LED
  // animation from stalling while a broker is unreachable.
  static constexpr uint16_t SOCKET_TIMEOUT_S = 1;

 private:
  void attemptConnect();
  void onConnected();
  void handleMessage(const String& topic, const String& payload);
  void publishStateIfChanged(bool force);
  String effectiveScope(const JsonDocument& state) const;

  NetworkService* network = nullptr;
  Controller* controller = nullptr;
  HomeAssistantConfig config;
  HomeAssistantIdentity identity;

  WiFiClient socket;
  PubSubClient client{socket};
  String clientId;
  String lastStatePayload;
  uint32_t lastAttemptAt = 0;
  uint32_t reconnectDelayMs = RECONNECT_MIN_MS;
  uint32_t lastPollAt = 0;
  bool configured = false;
  bool discoveryPublished = false;
};

#endif
