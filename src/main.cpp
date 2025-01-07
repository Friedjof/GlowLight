#include <Arduino.h>
#include <Button2.h>
#include <Wire.h>

// controller and services
#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"

// modes
#include "Alert.h"
#include "StaticMode.h"
#include "ColorPickerMode.h"
#include "RainbowMode.h"
#include "BeaconMode.h"
#include "CandleMode.h"
#include "MiniGame.h"

// config
#include "GlowConfig.h"


// services
Button2 button;

LightService lightService;
DistanceService distanceService;

// controller
Controller controller(&distanceService);

// light modes
Alert alertMode(&lightService, &distanceService);
StaticMode staticMode(&lightService, &distanceService);
ColorPickerMode colorPickerMode(&lightService, &distanceService);
RainbowMode rainbowMode(&lightService, &distanceService);
BeaconMode beaconMode(&lightService, &distanceService);
CandleMode candleMode(&lightService, &distanceService);
MiniGame miniGame(&lightService, &distanceService);

/*
 * This is the main setup function; it is called only once during startup.
 */
void setup() {
  Serial.begin(115200);

  // setup i2c for the distance sensor
  Wire.begin(DISTANCE_SENSOR_SDA, DISTANCE_SENSOR_SCL);

  Serial.println("[INFO] Starting Glow");

  // setup services
  lightService.setup();
  distanceService.setup();
  button.begin(BUTTON_PIN);

  // set debounce time (this is the time the button needs to be stable before a press is registered)
  button.setLongClickTime(500);

  // the modes need to be added to the controller and the order will be the order of the modes
  controller.addMode(&staticMode);
  controller.addMode(&colorPickerMode);
  controller.addMode(&rainbowMode);
  controller.addMode(&beaconMode);
  controller.addMode(&candleMode);
  controller.addMode(&miniGame);

  // set alert mode
  controller.setAlertMode(&alertMode);

  // setup controller
  controller.setup();

  // configure button handlers
  button.setLongClickHandler([](Button2 &btn) {
    controller.nextMode();
  });

  button.setClickHandler([](Button2 &btn) {
    controller.nextOption();
  });

  // this click can be used for custom actions in the current mode
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
}
