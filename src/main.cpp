#include <Arduino.h>
#include <Button2.h>
#include <Wire.h>

#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"

#include "Alert.h"
#include "StaticMode.h"
#include "ColorPickerMode.h"
#include "RainbowMode.h"
#include "CandleMode.h"
#include "BeaconMode.h"
#include "MiniGame.h"

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
CandleMode candleMode(&lightService, &distanceService);
BeaconMode beaconMode(&lightService, &distanceService);
MiniGame miniGame(&lightService, &distanceService);


void setup() {
  Serial.begin(115200);
  Wire.begin(DISTANCE_SENSOR_SDA, DISTANCE_SENSOR_SCL);

  Serial.println("[INFO] Starting Glow");

  // setup services
  lightService.setup();
  distanceService.setup();
  button.begin(BUTTON_PIN);

  button.setLongClickTime(500);

  // setup modes
  alertMode.setup();
  staticMode.setup();
  colorPickerMode.setup();
  rainbowMode.setup();
  candleMode.setup();
  beaconMode.setup();
  miniGame.setup();

  // add modes to controller
  controller.addMode(&beaconMode);
  
  controller.addMode(&staticMode);
  controller.addMode(&colorPickerMode);
  controller.addMode(&rainbowMode);
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

  button.setDoubleClickHandler([](Button2 &btn) {
    controller.customClick();
  });

  Serial.println("[INFO] Glow started");
}


void loop() {
  button.loop();
  controller.loop();
  lightService.loop();
  distanceService.loop();
}
