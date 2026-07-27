#include "DistanceService.h"

namespace {
constexpr uint8_t SENSOR_ADDRESS = 0x29;
constexpr uint8_t SENSOR_SETUP_ATTEMPTS = 8;
constexpr uint8_t SENSOR_MODEL_ID_REGISTER = 0xC0;
constexpr uint8_t MAX_CONSECUTIVE_MEASUREMENT_ERRORS = 3;
constexpr uint32_t SENSOR_RECOVERY_INTERVAL_MS = 5000;
constexpr uint8_t RETURN_SIGNAL_TO_AMBIENT_RATIO = 8;
constexpr uint16_t MIN_ADAPTIVE_MAX_DISTANCE_MM = DISTANCE_MAX_MM * 3 / 4;
}

void DistanceService::setup() {
  if (this->sensorPresent) {
    Serial.println("[INFO] Distance sensor is present");
    return;
  }

  for (uint8_t attempt = 1; attempt <= SENSOR_SETUP_ATTEMPTS; attempt++) {
    if (this->initializeSensor(attempt, SENSOR_SETUP_ATTEMPTS)) {
      return;
    }

    if (attempt < SENSOR_SETUP_ATTEMPTS) {
      delay(1000);
    }
  }

  this->nextRecoveryAttempt = millis() + SENSOR_RECOVERY_INTERVAL_MS;
  Serial.println("[ERROR] Distance sensor unavailable, recovery will be retried");
}

void DistanceService::recoverI2CBus() {
  Wire.end();

  pinMode(DISTANCE_SENSOR_SDA, INPUT_PULLUP);
  pinMode(DISTANCE_SENSOR_SCL, INPUT_PULLUP);
  delay(1);

  if (digitalRead(DISTANCE_SENSOR_SDA) == LOW) {
    pinMode(DISTANCE_SENSOR_SCL, OUTPUT_OPEN_DRAIN);
    digitalWrite(DISTANCE_SENSOR_SCL, HIGH);

    for (uint8_t pulse = 0; pulse < 9 && digitalRead(DISTANCE_SENSOR_SDA) == LOW; pulse++) {
      digitalWrite(DISTANCE_SENSOR_SCL, LOW);
      delayMicroseconds(5);
      digitalWrite(DISTANCE_SENSOR_SCL, HIGH);
      delayMicroseconds(5);
    }

    // Generate a STOP condition so a sensor interrupted mid-transfer releases SDA.
    pinMode(DISTANCE_SENSOR_SDA, OUTPUT_OPEN_DRAIN);
    digitalWrite(DISTANCE_SENSOR_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(DISTANCE_SENSOR_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(DISTANCE_SENSOR_SDA, HIGH);
    delayMicroseconds(5);
  }

  pinMode(DISTANCE_SENSOR_SDA, INPUT_PULLUP);
  pinMode(DISTANCE_SENSOR_SCL, INPUT_PULLUP);
  Wire.begin(DISTANCE_SENSOR_SDA, DISTANCE_SENSOR_SCL);
}

uint8_t DistanceService::probeSensor() {
  Wire.beginTransmission(SENSOR_ADDRESS);
  return Wire.endTransmission();
}

bool DistanceService::readRegister(uint8_t reg, uint8_t* value) {
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(SENSOR_ADDRESS, uint8_t(1)) != 1) {
    return false;
  }

  *value = Wire.read();
  return true;
}

bool DistanceService::initializeSensor(uint8_t attempt, uint8_t totalAttempts) {
  this->recoverI2CBus();
  uint8_t i2cError = this->probeSensor();
  uint8_t modelId = 0;
  bool modelIdReadable = this->readRegister(SENSOR_MODEL_ID_REGISTER, &modelId);

  if (i2cError == 0 &&
      this->sensor.begin(SENSOR_ADDRESS, false, &Wire,
                         Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE)) {
    this->sensorPresent = true;
    this->measurementConfirmed = false;
    this->consecutiveMeasurementErrors = 0;
    Serial.println("[INFO] Sensor initialized");
    return true;
  }

  Serial.printf(
    "[ERROR] Distance sensor initialization failed (attempt %u/%u, I2C=%u, model=0x%02X, readable=%d, driver=%d, SDA=%d, SCL=%d)\n",
    attempt, totalAttempts, i2cError, modelId, modelIdReadable,
    this->sensor.Status,
    digitalRead(DISTANCE_SENSOR_SDA), digitalRead(DISTANCE_SENSOR_SCL)
  );
  return false;
}

void DistanceService::loop() {
  if (!this->sensorPresent) {
    if (static_cast<int32_t>(millis() - this->nextRecoveryAttempt) >= 0) {
      if (!this->initializeSensor(1, 1)) {
        this->nextRecoveryAttempt = millis() + SENSOR_RECOVERY_INTERVAL_MS;
      }
    }
    return;
  }

  VL53L0X_RangingMeasurementData_t measure;
  VL53L0X_Error measurementError = this->sensor.rangingTest(&measure, false);

  if (measurementError != VL53L0X_ERROR_NONE) {
    this->consecutiveMeasurementErrors++;

    if (this->consecutiveMeasurementErrors >= MAX_CONSECUTIVE_MEASUREMENT_ERRORS) {
      Serial.printf("[ERROR] Distance sensor stopped responding (driver=%d), scheduling recovery\n",
                    measurementError);
      this->sensorPresent = false;
      this->measurementConfirmed = false;
      this->nextRecoveryAttempt = millis() + SENSOR_RECOVERY_INTERVAL_MS;
    }
    return;
  }

  this->consecutiveMeasurementErrors = 0;

  if (!this->measurementConfirmed) {
    this->measurementConfirmed = true;
    Serial.printf("[INFO] Distance sensor ranging (status=%u, distance=%u mm)\n",
                  measure.RangeStatus, measure.RangeMilliMeter);
  }

  this->result.status = measure.RangeStatus;

  // check if object is present
  bool wasPresent = this->objectPresent;
  this->objectPresent = this->isObjectPresent(measure.RangeMilliMeter);

  if (this->objectPresent && !wasPresent) {
    this->maximumReached = false;
    this->resetDistanceFilter();
  }

  if (this->objectPresent && millis() - this->lastWipe > QUICK_WIPE_TIMEOUT) {
    uint16_t filteredDistance = this->filter(measure.RangeMilliMeter);
    if (!this->distanceFilterReady()) {
      return;
    }

    bool strongReturnSignal = measure.SignalRateRtnMegaCps >=
      measure.AmbientRateRtnMegaCps * RETURN_SIGNAL_TO_AMBIENT_RATIO;

    if (filteredDistance >= DISTANCE_MAX_MM ||
        (filteredDistance >= MIN_ADAPTIVE_MAX_DISTANCE_MM && !strongReturnSignal)) {
      this->maximumReached = true;
    } else if (this->maximumReached && strongReturnSignal) {
      this->maximumReached = false;
    }

    this->result.distance = this->maximumReached
      ? DISTANCE_MAX_MM
      : filteredDistance;

    if (this->measurements <= QUICK_WIPE_MEASUREMENTS) {
      this->measurements++;
    }
  }

  // check if wipe is detected
  if (wasPresent && !this->objectPresent) {
    this->wipeDetected = this->measurements > 0 && this->measurements <= QUICK_WIPE_MEASUREMENTS;

    if (this->wipeDetected) {
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

  if (this->objectPresent) {
    uint16_t level = this->distance2level(this->result.distance);

    // if distance is unchanged, do nothing and if state not fixed, set it to changing
    if (level != this->result.level && !this->fixed()) {
      this->result.level = level;

      this->lastChange = millis();
      this->status = 0x01;

      this->levelChanged = true;
    }
  }

  if (this->changing() && !this->isObjectPresent()) {
    this->status = 0x00;
  }

  // Hold level if distance is not changing and is within range (hand is close to sensor)
  if (this->changing() && millis() - this->lastChange > DISTANCE_HOLD_MS && this->isObjectPresent()) {
    this->status = 0x02;
    this->sendAlert = true;
  }

  // Release if distance is not changing and is out of range (hand is far from sensor)
  if (this->fixed() && !this->isObjectPresent()) {
    this->status = 0x00;
    this->sendAlert = false;
  }

  if (this->objectDisappeared) {
    Serial.printf("[DEBUG] Object disappeared, holding distance %u mm (level %u)\n",
                  this->result.distance, this->result.level);
  }
}

uint16_t DistanceService::filter(uint16_t value) {
  this->distanceSamples[this->distanceSampleIndex] = value;
  this->distanceSampleIndex = (this->distanceSampleIndex + 1) % DISTANCE_FILTER_SAMPLES;
  if (this->distanceSampleCount < DISTANCE_FILTER_SAMPLES) {
    this->distanceSampleCount++;
  }

  if (!this->distanceFilterReady()) {
    return this->result.distance;
  }

  uint16_t a = this->distanceSamples[0];
  uint16_t b = this->distanceSamples[1];
  uint16_t c = this->distanceSamples[2];
  uint16_t median = max(min(a, b), min(max(a, b), c));

  if (abs(static_cast<int>(median) - static_cast<int>(this->result.distance)) >
      DISTANCE_THRESHOLD_MM) {
    return median;
  }

  return this->result.distance;
}

void DistanceService::resetDistanceFilter() {
  this->distanceSampleCount = 0;
  this->distanceSampleIndex = 0;
}

bool DistanceService::distanceFilterReady() {
  return this->distanceSampleCount == DISTANCE_FILTER_SAMPLES;
}

uint16_t DistanceService::distance2level(uint16_t distance) {
  if (distance > DISTANCE_UNCHANGED_MM) {
    return this->result.level;
  } else if (distance >= DISTANCE_MAX_MM) {
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

// Only measurements inside the configured interaction range represent a hand.
bool DistanceService::isObjectPresent(uint16_t distance) {
  return distance >= DISTANCE_MIN_MM && distance < DISTANCE_UNCHANGED_MM &&
         this->result.status == 0x00;
}

bool DistanceService::isObjectPresent() {
  return this->objectPresent;
}

// an object has disappeared if the object was present and is no longer present
bool DistanceService::hasObjectDisappeared() {
  return this->objectDisappeared;
}

// a wipe is detected if the object was present for a short period of time and then disappeared
bool DistanceService::hasWipeDetected() {
  return this->wipeDetected;
}

bool DistanceService::consumeLevelChange() {
  bool changed = this->levelChanged;
  this->levelChanged = false;
  return changed;
}

bool DistanceService::alert() {
  if (!this->sendAlert) {
    return false;
  }

  this->sendAlert = false;

  return true;
}
