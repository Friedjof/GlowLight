#ifndef DISTANCESERVICE_H
#define DISTANCESERVICE_H

#include <Arduino.h>

#include "Adafruit_VL53L0X.h"

#include "GlowConfig.h"

class CommunicationService;  // Forward declaration

typedef struct {
  uint16_t distance;
  uint16_t level;
  uint8_t status;
} result_t;


class DistanceService {
  public:
    DistanceService(CommunicationService* communicationService);
    ~DistanceService();

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

    bool alert();

    void setRemoteResult(uint16_t distance, uint16_t level);

  private:
    Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
    CommunicationService* communicationService;

    result_t result = {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS};

    uint8_t status = 0x00;
    bool sendAlert = false;

    uint64_t lastChange = 0;
    uint16_t measurements = 0;

    bool sensorPresent = false;
    bool objectPresent = false;
    bool objectDisappeared = false;

    bool wipeDetected = false;
    uint16_t numberOfWipes = 0;
    uint64_t lastWipe = 0;

    bool resultFromRemote = false;
};

#endif