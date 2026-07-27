#ifndef DISTANCESERVICE_H
#define DISTANCESERVICE_H

#include <Arduino.h>
#include <Wire.h>

#include "Adafruit_VL53L0X.h"

#include "GlowConfig.h"

typedef struct {
  uint16_t distance;
  uint16_t level;
  uint8_t status;
} result_t;


class DistanceService {
  public:
    void setup();
    void loop();

    uint16_t filter(uint16_t value);
    uint16_t distance2level(uint16_t distance);

    uint16_t getDistance();
    uint16_t getLevel();
    result_t getResult();

    uint16_t getNumberOfWipes();
    void setNumberOfWipes(uint16_t numberOfWipes);

    bool fixed();
    bool changing();
    bool released();

    bool isObjectPresent();
    bool isObjectPresent(uint16_t distance);
    bool hasObjectDisappeared();
    bool hasWipeDetected();
    bool consumeLevelChange();

    bool alert();

  private:
    static constexpr uint8_t DISTANCE_FILTER_SAMPLES = 3;

    void recoverI2CBus();
    uint8_t probeSensor();
    bool readRegister(uint8_t reg, uint8_t* value);
    bool initializeSensor(uint8_t attempt, uint8_t totalAttempts);
    void resetDistanceFilter();
    bool distanceFilterReady();

    Adafruit_VL53L0X sensor = Adafruit_VL53L0X();

    result_t result = {0, LED_DEFAULT_BRIGHTNESS};

    uint8_t status = 0x00;
    bool sendAlert = false;

    uint64_t lastChange = 0;
    uint16_t measurements = 0;
    uint16_t distanceSamples[DISTANCE_FILTER_SAMPLES] = {0};
    uint8_t distanceSampleCount = 0;
    uint8_t distanceSampleIndex = 0;

    bool sensorPresent = false;
    bool measurementConfirmed = false;
    uint8_t consecutiveMeasurementErrors = 0;
    uint64_t nextRecoveryAttempt = 0;
    bool objectPresent = false;
    bool maximumReached = false;
    bool objectDisappeared = false;

    bool wipeDetected = false;
    uint16_t numberOfWipes = 0;
    uint64_t lastWipe = 0;

    bool levelChanged = false;
};

#endif
