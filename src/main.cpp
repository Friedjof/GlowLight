#include <Arduino.h>
#include <Button2.h>
#include <Wire.h>

// Controller and services
#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"
#include "CommunicationService.h"
#include "IntegrationConsole.h"
#include "ModeRegistration.h"

// Config
#include "GlowConfig.h"

// Services
Button2 button;

LightService lightService;
CommunicationService communicationService;
DistanceService distanceService;

// Controller
Controller controller(&distanceService, &communicationService);

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

  registerModes(controller, lightService, distanceService, communicationService);

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
  integrationConsoleLoop(controller);
  button.loop();
  distanceService.loop();
  controller.loop();
  lightService.loop();
  communicationService.loop();
}
