/*
 * HomeAssistantProtocol.h
 *
 * Translates between the transport-neutral control API and MQTT.
 *
 * Everything here is derived from the documents the Controller already
 * produces: `capabilities()` describes the modes, their options and their
 * settings, `state()` describes the current values. A new mode therefore
 * appears in Home Assistant without a single line of code in this file.
 *
 * The functions are pure and free of hardware and MQTT dependencies, so the
 * whole mapping can be tested on the host.
 */

#ifndef HOMEASSISTANTPROTOCOL_H
#define HOMEASSISTANTPROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

struct HomeAssistantIdentity {
  String deviceId;          // stable per lamp, used in topics and unique IDs
  String deviceName;        // shown in Home Assistant
  String baseTopic;         // "glowlight/<deviceId>"
  String discoveryPrefix;   // "homeassistant"
  String firmwareVersion;
};

// Receives one message at a time so no more than a single payload is ever held
// in memory. Returning false aborts the sequence.
using HomeAssistantSink =
    std::function<bool(const String& topic, const String& payload, bool retain)>;

namespace HomeAssistantProtocol {

// Topic of the device-wide online/offline marker, also used as the last will.
String availabilityTopic(const HomeAssistantIdentity& identity);
// Topic carrying the complete state document, useful for templates.
String stateTopic(const HomeAssistantIdentity& identity);
// Per-mode marker so entities of an inactive mode disappear from the dashboard.
String modeAvailabilityTopic(const HomeAssistantIdentity& identity,
                             const String& modeId);

// Retained discovery configuration for every entity this lamp offers.
bool buildDiscovery(const HomeAssistantIdentity& identity,
                    const JsonDocument& capabilities, const HomeAssistantSink& emit);

// Current values for every entity, including the per-mode availability markers.
bool buildState(const HomeAssistantIdentity& identity,
                const JsonDocument& capabilities, const JsonDocument& state,
                const HomeAssistantSink& emit);

// Command topics that have to be subscribed to.
void buildSubscriptions(const HomeAssistantIdentity& identity,
                        const JsonDocument& capabilities,
                        const std::function<void(const String&)>& subscribe);

// Turns an inbound command into a control API request. Returns false when the
// topic is unknown or the payload cannot be mapped.
bool buildControlRequest(const HomeAssistantIdentity& identity,
                         const JsonDocument& capabilities,
                         const JsonDocument& state, const String& topic,
                         const String& payload, const String& scope,
                         JsonDocument* request);

}  // namespace HomeAssistantProtocol

#endif
