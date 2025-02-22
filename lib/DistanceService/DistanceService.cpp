#include "DistanceService.h"

DistanceService::DistanceService() {
  this->sensorPresent = false;
  this->result.distance = 0;
}

DistanceService::~DistanceService() {
  // nothing to do here
}

void DistanceService::setup() {
  if (this->sensorPresent) {
    Serial.println("[INFO] Distance sensor is present");
    return;
  }

  uint8_t tries = 0;

  while (!this->sensor.begin() && tries < 8) {
    Serial.print("[ERROR] Failed to detect and initialize sensor, retrying in 1 second (retry ");
    Serial.print(++tries);
    Serial.println(")");

    delay(1000);
  }

  if (tries >= 8) {
    Serial.println("[ERROR] Failed to detect and initialize sensor, this functionality will be disabled");
    return;
  }

  // set sensor to high speed mode, alternatively use VL53L0X_SENSE_DEFAULT or VL53L0X_SENSE_LONG_RANGE
  this->sensor.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED);

  Serial.println("[INFO] Sensor initialized");

  this->sensorPresent = true;
}

void DistanceService::loop() {
  if (!this->sensorPresent) {
    return;
  }

  VL53L0X_RangingMeasurementData_t measure;

  this->sensor.rangingTest(&measure, false);

  uint16_t oldDistance = this->result.distance;
  this->result.status = measure.RangeStatus;

  // check if object is present
  bool wasPresent = this->objectPresent;
  this->objectPresent = this->isObjectPresent();

  if (this->objectPresent && millis() - this->lastWipe > QUICK_WIPE_TIMEOUT) {
    this->result.distance = this->filter(measure.RangeMilliMeter);

    if (this->measurements <= QUICK_WIPE_MEASUREMENTS) {
      this->measurements++;
    }
  }

  // check if wipe is detected
  if (wasPresent && !this->objectPresent) {
    this->wipeDetected = this->measurements > 0 && this->measurements <= QUICK_WIPE_MEASUREMENTS;

    if (this->wipeDetected) {
      this->result.distance = this->result.distance == DISTANCE_MAX_MM ? 0 : DISTANCE_MAX_MM;

      if (this->numberOfWipes < QUICK_WIPE_MAX) {
        this->numberOfWipes++;
      } else {
        this->numberOfWipes = 0;
      }
      
      this->lastWipe = millis();

      Serial.printf("[DEBUG] Wipe detected (%d)\n", this->numberOfWipes);
    }

    this->measurements = 0;
  } else if (this->wipeDetected) {
    this->wipeDetected = false;
  }

  this->objectDisappeared = wasPresent && !this->objectPresent && !this->wipeDetected;

  uint16_t level = this->distance2level(this->result.distance);

  // if distance is unchanged, do nothing and if state not fixed, set it to changing
  if (level != this->result.level && !this->fixed()) {
    this->result.level = level;

    this->lastChange = millis();
    this->status = 0x01;

    Serial.print("[DEBUG] Distance: ");
    Serial.print(this->result.distance);
    Serial.print(" mm, Level: ");
    Serial.println(this->result.level);
  }

  if (this->changing() && !this->isObjectPresent()) {
    this->status = 0x00;
  }

  // Hold level if distance is not changing and is within range (hand is close to sensor)
  if (this->changing() && millis() - this->lastChange > DISTANCE_HOLD_MS && this->isObjectPresent()) {
    this->status = 0x02;
    this->sendAlert = true;

    Serial.println("[DEBUG] Hold level");
  }

  // Release if distance is not changing and is out of range (hand is far from sensor)
  if (this->fixed() && !this->isObjectPresent()) {
    this->status = 0x00;
    this->sendAlert = false;

    Serial.println("[DEBUG] Release level");
  }

  if (this->objectDisappeared) {
    Serial.println("[DEBUG] Object disappeared");
  }
}

uint16_t DistanceService::filter(uint16_t value) {
  if (abs((int)value - (int)this->result.distance) > DISTANCE_THRESHOLD_MM) {
    return value;
  }

  return this->result.distance;
}

uint16_t DistanceService::distance2level(uint16_t distance) {
  if (distance > DISTANCE_UNCHANGED_MM) {
    return this->result.level;
  } else if (distance > DISTANCE_MAX_MM) {
    return DISTANCE_LEVELS;
  } else if (distance < DISTANCE_MIN_MM) {
    return 0;
  }

  double input = map(distance, DISTANCE_MIN_MM, DISTANCE_MAX_MM, 0, DISTANCE_LEVELS);

  double normalized = input / DISTANCE_LEVELS;
  double result = exp(normalized * log(1 + DISTANCE_LEVELS)) - 1;

  return (uint16_t)result;
}

uint16_t DistanceService::getDistance() {
  if (!this->sensorPresent) {
    return DISTANCE_MAX_MM;
  }
  
  return this->result.distance;
}

uint16_t DistanceService::getLevel() {
  if (!this->sensorPresent) {
    return LED_MAX_BRIGHTNESS;
  }

  return this->result.level;
}

result_t DistanceService::getResult() {
  if (!this->sensorPresent) {
    return {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS};
  }

  return this->result;
}

uint16_t DistanceService::getNumberOfWipes() {
  return this->numberOfWipes;
}

void DistanceService::setNumberOfWipes(uint16_t numberOfWipes) {
  this->numberOfWipes = numberOfWipes;
}

bool DistanceService::fixed() {
  return this->status == DISTANCE_HOLD_STATUS;
}

bool DistanceService::changing() {
  return this->status == DISTANCE_CHANGING_STATUS;
}

bool DistanceService::released() {
  return this->status == DISTANCE_RELEASE_STATUS;
}

// an object is present if the distance is less than the unchanged distance and the status is 0x00 (valid)
bool DistanceService::isObjectPresent(uint16_t distance) {
  return distance < DISTANCE_UNCHANGED_MM && this->result.status == 0x00;
}

bool DistanceService::isObjectPresent() {
  return this->isObjectPresent(this->result.distance);
}

// an object has disappeared if the object was present and is no longer present
bool DistanceService::hasObjectDisappeared() {
  return this->objectDisappeared;
}

// a wipe is detected if the object was present for a short period of time and then disappeared
bool DistanceService::hasWipeDetected() {
  return this->wipeDetected;
}

bool DistanceService::alert() {
  if (!this->sendAlert) {
    return false;
  }

  this->sendAlert = false;

  return true;
}