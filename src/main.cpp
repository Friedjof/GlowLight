#include <Arduino.h>
#include <Button2.h>
#include <Wire.h>

// Controller and services
#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"
#include <WiFi.h>

#include "NetworkService.h"
#include "CommunicationService.h"
#include "ConfigService.h"
#include "PreferencesConfigStore.h"
#include "CaptivePortalService.h"
#include "OtaService.h"
#include "HomeAssistantService.h"
#include "IntegrationConsole.h"
#include "ModeRegistration.h"

// Config
#include "GlowConfig.h"

// Shown as the device firmware version in Home Assistant.
#ifndef GLOW_FIRMWARE_VERSION
#define GLOW_FIRMWARE_VERSION "dev"
#endif

// Services
Button2 button;

LightService lightService;
NetworkService networkService;
CommunicationService communicationService;
DistanceService distanceService;
PreferencesConfigStore configStore;
CaptivePortalService captivePortalService;
OtaService otaService;
HomeAssistantService homeAssistantService;

// Controller
Controller controller(&distanceService, &communicationService);

/*
 * This is the main setup function; it is called only once during startup.
 */
void setup() {
#ifdef GLOW_INTEGRATION_TEST
  // A single injected ESP-NOW frame is a 507 character console line, which does
  // not fit the default receive buffer.
  Serial.setRxBufferSize(2048);
#endif
  Serial.begin(115200);

  // Setup I2C for the distance sensor
  Wire.begin(DISTANCE_SENSOR_SDA, DISTANCE_SENSOR_SCL);

  Serial.println("[INFO] Starting Glow");

  DeviceConfig deviceConfig = DeviceConfig::compileTimeDefaults();
  if (configStore.load(&deviceConfig)) {
    Serial.println("[INFO] Loaded persistent device configuration");
  } else {
    Serial.println("[INFO] Using compile-time device configuration");
  }
  String configError;
  if (!deviceConfig.validate(&configError)) {
    Serial.printf("[ERROR] Device configuration rejected: %s\n",
                  configError.c_str());
    deviceConfig = DeviceConfig::safeDefaults();
  }

  // Every lamp is flashed from the same configuration, so a default hostname
  // would be identical everywhere. Deriving it once here makes the mDNS name and
  // the Home Assistant device name unique, because both follow this value.
  uint8_t localMac[6] = {};
  WiFi.macAddress(localMac);
  deviceConfig.hostname = DeviceConfig::uniqueHostname(deviceConfig.hostname, localMac);

  NetworkConfig networkConfig;
  networkConfig.enabled = deviceConfig.wifiEnabled;
  networkConfig.ssid = deviceConfig.wifiSsid;
  networkConfig.password = deviceConfig.wifiPassword;
  networkConfig.hostname = deviceConfig.hostname;
  networkConfig.fallbackChannel = deviceConfig.fallbackChannel;

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool portalRequested = GLOW_PORTAL_ENABLED && digitalRead(BUTTON_PIN) == LOW;
  String portalSsid = deviceConfig.hostname.substring(0, 26) + "-setup";

  // NetworkService owns the shared radio and must configure station mode before
  // ESP-NOW starts. Joining an access point continues asynchronously.
  lightService.setup();
  distanceService.setup();
  bool portalStarted = portalRequested &&
      networkService.setupProvisioning(networkConfig, portalSsid,
                                       GLOW_PORTAL_PASSWORD);
  if (!portalStarted) networkService.setup(networkConfig);

  CommunicationConfig communicationConfig;
  communicationConfig.enabled = deviceConfig.communicationEnabled;
  communicationConfig.groupKeyHex = deviceConfig.groupKeyHex;
  communicationService.setup(communicationConfig);
  networkService.onChannelChanged([](uint8_t channel) {
    communicationService.radioChannelChanged(channel);
    controller.requestResync();
  });

  button.begin(BUTTON_PIN);

  // Set debounce time (this is the time the button needs to be stable before a press is registered)
  button.setLongClickTime(500);

  registerModes(controller, lightService, distanceService, communicationService);

  // Setup controller
  controller.configureSyncDefaults(deviceConfig.syncFollow,
                                   deviceConfig.syncPublish);
  controller.configureRuntimeFeatures(GLOW_PORTAL_ENABLED,
                                      deviceConfig.otaEnabled);
  controller.setup();
  if (portalStarted) {
    captivePortalService.setup(configStore, networkService, deviceConfig);
  } else {
    OtaConfig otaConfig;
    otaConfig.enabled = deviceConfig.otaEnabled;
    otaConfig.password = deviceConfig.otaPassword;
    otaService.setup(networkService, otaConfig);

    // Home Assistant shares the infrastructure connection with OTA and is only
    // meaningful once the lamp is on the network, so it stays off in the portal.
    HomeAssistantConfig homeAssistantConfig;
    homeAssistantConfig.enabled = deviceConfig.mqttEnabled;
    homeAssistantConfig.host = deviceConfig.mqttHost;
    homeAssistantConfig.port = deviceConfig.mqttPort;
    homeAssistantConfig.user = deviceConfig.mqttUser;
    homeAssistantConfig.password = deviceConfig.mqttPassword;
    homeAssistantConfig.discoveryPrefix = deviceConfig.mqttDiscoveryPrefix;
    char deviceId[24];
    snprintf(deviceId, sizeof(deviceId), "glow-%u", communicationService.getNodeId());
    homeAssistantService.setup(networkService, controller, homeAssistantConfig,
                               deviceId, GLOW_FIRMWARE_VERSION);
  }

  // Configure button handlers
  button.setLongClickHandler([](Button2 &btn) {
    controller.nextMode();
  });

  button.setClickHandler([](Button2 &btn) {
    controller.nextOption();
  });

  // This click can be used for custom actions in the current mode
  button.setDoubleClickHandler([](Button2 &btn) {
    controller.customClick();
  });

  Serial.println("[INFO] GlowLight started");
}

/*
 * This is the main loop function; it is called repeatedly by the system.
 */
void loop() {
  // The services and controller need to be looped
  integrationConsoleLoop(controller, communicationService);
  button.loop();
  distanceService.loop();
  controller.loop();
  lightService.loop();
  networkService.loop();
  captivePortalService.loop();
  otaService.loop();
  homeAssistantService.loop();
  communicationService.loop();
}
