#ifndef DISTANCESERVICE_H
#define DISTANCESERVICE_H

#include <Arduino.h>

#include "Adafruit_VL53L0X.h"

#include "GlowConfig.h"


typedef struct {
  uint16_t distance;
  uint16_t level;
  uint8_t status;
} result_t;


class DistanceService {
  public:
    DistanceService();
    ~DistanceService();

    void setup();
    void loop();

    uint16_t filter(uint16_t value);
    uint16_t distance2level(uint16_t distance);

    uint16_t getDistance();
    uint16_t getLevel();
    result_t getResult();

    bool fixed();
    bool changing();
    bool released();
    bool objectPresent();
    bool alert();

  private:
    Adafruit_VL53L0X sensor = Adafruit_VL53L0X();

    result_t result = {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS};

    uint8_t status = 0x00;
    bool sendAlert = false;

    uint64_t lastChange = 0;

    bool sensorPresent = false;
};

#endif