#include <Arduino.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include "Controller.h"

#include "LinkService.h"
#include "GlowRegistry.h"

#include "LightService.h"
#include "DistanceService.h"
#include "ButtonService.h"

LinkService linkService;
GlowRegistry registry(&linkService);

LightService lightService;
DistanceService distanceService;
ButtonService buttonService;

Controller controller(&registry, &lightService, &distanceService, &buttonService, &linkService);

// ---- Function Declarations ----
void onDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len);
void simpleClickHandler(Button2 &b);
void doubleClickHandler(Button2 &b);
void longClickHandler(Button2 &b);


void setup() {
  Serial.begin(115200);

  // ---- Setup Services ----
  linkService.setup();
  registry.setup();

  lightService.setup();
  distanceService.setup();
  buttonService.setup();

  // ---- Setup Controller ----
  controller.setup();

  // ---- Register Callbacks ----
  // Register ESP-NOW receive callback
  esp_now_register_recv_cb(onDataRecv);
  // Set button event handlers
  buttonService.setSimpleClickHandler(simpleClickHandler);
  buttonService.setDoubleClickHandler(doubleClickHandler);
  buttonService.setLongClickHandler(longClickHandler);
  // Set registry data handler
  linkService.setDataHandler([](JsonDocument doc) {
    registry.receive(doc);
  });
  // Set controller command handler
  linkService.setCommandHandler([](JsonDocument doc) {
    controller.command(doc);
  });

  // ---- Finalize Setup ----
  Serial.println("ESP-NOW Initialized");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  controller.loop();
}

// ---- Callbacks ----
// Callback when data is received
void onDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  controller.receive(esp_now_info, incomingData, len);
}

// Callbacks for button events
void simpleClickHandler(Button2 &b) {
  controller.simpleClickHandler();
}

void doubleClickHandler(Button2 &b) {
  controller.doubleClickHandler();
}

void longClickHandler(Button2 &b) {
  controller.longClickHandler();
}
