#include "HomeAssistantService.h"

namespace {
const char AVAILABLE[] = "online";
const char UNAVAILABLE[] = "offline";
}  // namespace

bool HomeAssistantService::setup(NetworkService& networkService,
                                 Controller& controllerReference,
                                 const HomeAssistantConfig& configuration,
                                 const String& deviceId,
                                 const String& firmwareVersion) {
  this->network = &networkService;
  this->controller = &controllerReference;
  this->config = configuration;

  if (!this->config.enabled) return true;
  if (this->config.host.isEmpty() || this->config.port == 0) {
    Serial.println("[ERROR] Home Assistant disabled: broker host or port missing");
    return false;
  }
  if (this->config.discoveryPrefix.isEmpty()) this->config.discoveryPrefix = "homeassistant";
  if (this->config.baseTopicPrefix.isEmpty()) this->config.baseTopicPrefix = "glowlight";

  this->identity.deviceId = deviceId;
  this->identity.deviceName = networkService.hostname();
  this->identity.baseTopic = this->config.baseTopicPrefix + "/" + deviceId;
  this->identity.discoveryPrefix = this->config.discoveryPrefix;
  this->identity.firmwareVersion = firmwareVersion;
  this->clientId = deviceId;

  this->client.setServer(this->config.host.c_str(), this->config.port);
  this->client.setBufferSize(MQTT_BUFFER_SIZE);
  this->client.setSocketTimeout(SOCKET_TIMEOUT_S);
  this->client.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) body += static_cast<char>(payload[i]);
    this->handleMessage(String(topic), body);
  });

  this->configured = true;
  Serial.printf("[INFO] Home Assistant enabled, broker %s:%u, base topic %s\n",
                this->config.host.c_str(), this->config.port,
                this->identity.baseTopic.c_str());
  return true;
}

bool HomeAssistantService::connected() const {
  return this->configured && const_cast<PubSubClient&>(this->client).connected();
}

void HomeAssistantService::loop() {
  if (!this->configured || this->network == nullptr) return;

  if (!this->network->isConnected()) {
    if (this->client.connected()) this->client.disconnect();
    // A fresh connection has to republish everything the broker forgot.
    this->discoveryPublished = false;
    return;
  }

  if (!this->client.connected()) {
    uint32_t now = millis();
    if (now - this->lastAttemptAt < this->reconnectDelayMs) return;
    this->lastAttemptAt = now;
    this->attemptConnect();
    return;
  }

  this->client.loop();

  uint32_t now = millis();
  if (now - this->lastPollAt >= STATE_POLL_MS) {
    this->lastPollAt = now;
    this->publishStateIfChanged(false);
  }
}

void HomeAssistantService::attemptConnect() {
  String availability = HomeAssistantProtocol::availabilityTopic(this->identity);
  const char* user = this->config.user.isEmpty() ? nullptr : this->config.user.c_str();
  const char* password =
      this->config.password.isEmpty() ? nullptr : this->config.password.c_str();

  // The last will makes the lamp disappear from Home Assistant when it drops
  // off the network without saying goodbye.
  bool ok = this->client.connect(this->clientId.c_str(), user, password,
                                 availability.c_str(), 0, true, UNAVAILABLE);
  if (!ok) {
    this->reconnectDelayMs =
        this->reconnectDelayMs >= RECONNECT_MAX_MS / 2 ? RECONNECT_MAX_MS
                                                       : this->reconnectDelayMs * 2;
    Serial.printf("[WARN] MQTT connect failed (state %d), retrying in %u ms\n",
                  this->client.state(), this->reconnectDelayMs);
    return;
  }

  this->reconnectDelayMs = RECONNECT_MIN_MS;
  Serial.println("[INFO] MQTT connected");
  this->onConnected();
}

void HomeAssistantService::onConnected() {
  JsonDocument capabilities = this->controller->capabilities();

  if (!this->discoveryPublished) {
    bool complete = HomeAssistantProtocol::buildDiscovery(
        this->identity, capabilities,
        [this](const String& topic, const String& payload, bool retain) {
          if (this->client.publish(topic.c_str(), payload.c_str(), retain)) return true;
          Serial.printf("[WARN] Discovery message too large or broker refused: %s\n",
                        topic.c_str());
          return false;
        });
    this->discoveryPublished = complete;
    if (!complete) Serial.println("[WARN] Home Assistant discovery incomplete");
  }

  HomeAssistantProtocol::buildSubscriptions(
      this->identity, capabilities,
      [this](const String& topic) { this->client.subscribe(topic.c_str()); });

  this->client.publish(HomeAssistantProtocol::availabilityTopic(this->identity).c_str(),
                       AVAILABLE, true);
  this->publishStateIfChanged(true);
}

String HomeAssistantService::effectiveScope(const JsonDocument& state) const {
  if (this->config.commandScope != "group") return "local";
  // Only a lamp that is actually in sync and allowed to publish can carry a
  // command to the group; otherwise the command applies to this lamp alone.
  if (state["sync"]["status"].as<String>() != "synchronized") return "local";
  if (!state["sync"]["publish"].as<bool>()) return "local";
  return "group";
}

void HomeAssistantService::handleMessage(const String& topic, const String& payload) {
  JsonDocument state = this->controller->state();
  JsonDocument capabilities = this->controller->capabilities();

  JsonDocument request;
  if (!HomeAssistantProtocol::buildControlRequest(this->identity, capabilities, state,
                                                  topic, payload,
                                                  this->effectiveScope(state), &request)) {
    Serial.printf("[WARN] Ignoring unmapped MQTT command on %s\n", topic.c_str());
    return;
  }

  JsonDocument response = this->controller->executeControl(request);
  if (!response["ok"].as<bool>()) {
    Serial.printf("[WARN] MQTT command rejected: %s\n",
                  response["error"]["code"].as<String>().c_str());
  }
  // Report the result immediately so Home Assistant does not keep an optimistic
  // value that the lamp refused.
  this->publishStateIfChanged(true);
}

void HomeAssistantService::publishStateIfChanged(bool force) {
  if (!this->client.connected()) return;

  JsonDocument state = this->controller->state();
  String serialized;
  serializeJson(state, serialized);
  if (!force && serialized == this->lastStatePayload) return;
  this->lastStatePayload = serialized;

  JsonDocument capabilities = this->controller->capabilities();
  HomeAssistantProtocol::buildState(
      this->identity, capabilities, state,
      [this](const String& topic, const String& payload, bool retain) {
        return this->client.publish(topic.c_str(), payload.c_str(), retain);
      });
}
