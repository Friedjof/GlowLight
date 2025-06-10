#include <Arduino.h>
#include <Button2.h>
#include <Wire.h>

// Controller and services
#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"
#include "CommunicationService.h"

// Modes
#include "Alert.h"
#include "StaticMode.h"
#include "ColorPickerMode.h"
#include "RainbowMode.h"
#include "RandomGlowMode.h"
//#include "BeaconMode.h"
//#include "CandleMode.h"
//#include "SunsetMode.h"
//#include "StrobeMode.h"
//#include "MiniGame.h"

// Config
#include "GlowConfig.h"

// Scheduler
Scheduler scheduler;

// Services
Button2 button;

LightService lightService;
DistanceService distanceService;
CommunicationService communicationService(&scheduler);

// Controller
Controller controller(&distanceService, &communicationService);

// Light modes
Alert alertMode(&lightService, &distanceService, &communicationService);
StaticMode staticMode(&lightService, &distanceService, &communicationService);
ColorPickerMode colorPickerMode(&lightService, &distanceService, &communicationService);
RainbowMode rainbowMode(&lightService, &distanceService, &communicationService);
RandomGlowMode randomGlowMode(&lightService, &distanceService, &communicationService);
//BeaconMode beaconMode(&lightService, &distanceService, &communicationService);
//CandleMode candleMode(&lightService, &distanceService, &communicationService);
//SunsetMode sunsetMode(&lightService, &distanceService, &communicationService);
//StrobeMode strobeMode(&lightService, &distanceService, &communicationService);
//MiniGame miniGame(&lightService, &distanceService, &communicationService);

/*
 * This is the main setup function; it is called only once during startup.
 */
void setup() {
  Serial.begin(115200);

  // Setup I2C for the distance sensor
  Wire.begin(DISTANCE_SENSOR_SDA, DISTANCE_SENSOR_SCL);

  Serial.println("[INFO] Starting Glow");

  // Setup services
  lightService.setup();
  distanceService.setup();
  communicationService.setup();

  button.begin(BUTTON_PIN);

  // Set debounce time (this is the time the button needs to be stable before a press is registered)
  button.setLongClickTime(500);

  // The modes need to be added to the controller and the order will be the order of the modes
  controller.addMode(&staticMode);
  controller.addMode(&colorPickerMode);
  controller.addMode(&rainbowMode);
  controller.addMode(&randomGlowMode);
  //controller.addMode(&beaconMode);
  //controller.addMode(&candleMode);
  //controller.addMode(&sunsetMode);
  //controller.addMode(&strobeMode);
  //controller.addMode(&miniGame);

  // Set alert mode
  controller.setAlertMode(&alertMode);

  // Setup controller
  controller.setup();

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
  button.loop();
  controller.loop();
  lightService.loop();
  distanceService.loop();
  communicationService.loop();
}
