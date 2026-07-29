#include "SunsetMode.h"

// Duration options: 5min, 15min, 30min, 60min
const uint32_t SunsetMode::DURATION_OPTIONS[4] = {
  5 * 60 * 1000,   // 5 minutes
  15 * 60 * 1000,  // 15 minutes
  30 * 60 * 1000,  // 30 minutes
  60 * 60 * 1000   // 60 minutes
};

const String SunsetMode::DURATION_NAMES[4] = {
  "5 min",
  "15 min", 
  "30 min",
  "60 min"
};

// Color definitions for each phase
const CRGB SunsetMode::PHASE_COLORS[5] = {
  CRGB(255, 220, 180), // Warm white (start)
  CRGB(255, 200, 120), // Golden yellow
  CRGB(255, 140, 60),  // Orange
  CRGB(180, 40, 20),   // Deep red
  CRGB(0, 0, 0)        // Off (end)
};

// Phase boundaries: 25%, 50%, 85%, 100%
const float SunsetMode::PHASE_BOUNDARIES[4] = {0.25f, 0.50f, 0.85f, 1.0f};

SunsetMode::SunsetMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) 
  : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Sunset";
  this->id = "sunset";
  this->description = "Natural sunset simulation for bedtime";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "GPL-3.0";
}

void SunsetMode::setup() {
  // Initialize registry values
  this->registry.init("duration", RegistryType::INT, 1, 0, 3); // Default to 15min (index 1)
  this->registry.init("manual_shutdown", RegistryType::BOOL, false);
  this->registry.init("sunset_active", RegistryType::BOOL, false);
  this->markSettingReadOnly("manual_shutdown");
  this->markSettingReadOnly("sunset_active");

  JsonDocument startArguments;
  startArguments["duration"]["type"] = "integer";
  startArguments["duration"]["minimum"] = 0;
  startArguments["duration"]["maximum"] = 3;
  startArguments["duration"]["required"] = true;
  this->declareCommand("start", startArguments);
  JsonDocument noArguments;
  noArguments.to<JsonObject>();
  this->declareCommand("shutdown", noArguments);

  // Set initial values
  this->currentDuration = this->registry.getInt("duration");
  this->sunsetDurationMs = DURATION_OPTIONS[this->currentDuration];
  this->isManualShutdown = this->registry.getBool("manual_shutdown");
  this->sunsetActive = this->registry.getBool("sunset_active");
  this->currentPhase = GOLDEN_HOUR;

  // Add mode options
  this->addOption("Brightness", std::function<void()>([this](){ this->updateBrightnessFromSensor(); }));
  this->addOption("Duration", std::function<void()>([this](){ this->newDuration(); }));
}

void SunsetMode::customFirst() {
  // Reset state when mode is first selected
  this->isManualShutdown = false;
  this->registry.setBool("manual_shutdown", false);
  
  // Start new sunset if not manually shut down
  if (!this->isManualShutdown) {
    this->startSunset(false);
    this->showDurationFeedback();
  }
}

void SunsetMode::customLoop() {
  // If manually shut down, stay off
  if (this->isManualShutdown) {
    this->lightService->fill(CRGB::Black);
    return;
  }

  // If sunset not active, stay off
  if (!this->sunsetActive) {
    this->lightService->fill(CRGB::Black);
    return;
  }

  // Calculate sunset progress
  float progress = this->getSunsetProgress();
  
  // Check if sunset is complete
  if (progress >= 1.0f) {
    this->sunsetActive = false;
    this->registry.setBool("sunset_active", false);
    this->currentPhase = COMPLETE;
    this->lightService->fill(CRGB::Black);
    Serial.println("[SunsetMode] Sunset complete - entering sleep mode");
    return;
  }

  // Update current phase
  this->currentPhase = this->getCurrentPhase(progress);

  // Calculate and set colors
  CRGB sunsetColor = this->calculateSunsetColor(progress);
  uint8_t effectBrightness = this->calculateBrightness(progress);
  
  // The fade is part of the effect; global user brightness is applied by LightService.
  sunsetColor.nscale8(effectBrightness);
  
  // Set all LEDs to the sunset color
  for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
    this->lightService->setLed(i, sunsetColor);
  }
}

void SunsetMode::last() {
  // Save current state to registry
  this->registry.setBool("manual_shutdown", this->isManualShutdown);
  this->registry.setBool("sunset_active", this->sunsetActive);
}

void SunsetMode::customClick() {
  // Double click: Force complete sunset and stay off
  this->isManualShutdown = true;
  this->sunsetActive = false;
  this->currentPhase = COMPLETE;
  this->registry.setBool("manual_shutdown", true);
  this->registry.setBool("sunset_active", false);
  this->lightService->fill(CRGB::Black);
  
  Serial.println("[SunsetMode] Manual shutdown - staying off until mode change");
  
  // Broadcast shutdown to synchronized lamps
  this->broadcastSunsetShutdown();
}

bool SunsetMode::handleRemoteCommand(const String& command, const JsonDocument& payload) {
  if (command == "start") {
    if (!payload["duration"].is<int>()) {
      return false;
    }

    int duration = payload["duration"].as<int>();
    if (duration < 0 || duration > 3) {
      return false;
    }

    this->currentDuration = duration;
    this->sunsetDurationMs = DURATION_OPTIONS[this->currentDuration];
    this->sunsetStartTime = millis();
    this->currentPhase = GOLDEN_HOUR;
    this->isManualShutdown = false;
    this->sunsetActive = true;
    this->registry.setInt("duration", this->currentDuration);
    this->registry.setBool("manual_shutdown", false);
    this->registry.setBool("sunset_active", true);
    return true;
  }

  if (command == "shutdown") {
    this->isManualShutdown = true;
    this->sunsetActive = false;
    this->currentPhase = COMPLETE;
    this->registry.setBool("manual_shutdown", true);
    this->registry.setBool("sunset_active", false);
    this->lightService->fill(CRGB::Black);
    return true;
  }

  return false;
}

void SunsetMode::onStateApplied() {
  this->currentDuration = this->registry.getInt("duration");
  this->sunsetDurationMs = DURATION_OPTIONS[this->currentDuration];
  this->isManualShutdown = this->registry.getBool("manual_shutdown");
  this->sunsetActive = this->registry.getBool("sunset_active");
  if (this->sunsetActive) this->sunsetStartTime = millis();
}

void SunsetMode::onStateActivated() {
  this->currentPhase = this->sunsetActive ? GOLDEN_HOUR : COMPLETE;
  if (this->sunsetActive) this->sunsetStartTime = millis();
  if (this->isManualShutdown || !this->sunsetActive) {
    this->lightService->fill(CRGB::Black);
  }
}

bool SunsetMode::newDuration() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  // Use distance to select duration (4 levels)
  uint16_t level = this->getLevel();
  uint8_t newDuration = map(level, 0, DISTANCE_LEVELS, 0, 3);
  
  if (newDuration == this->currentDuration) {
    return false;
  }

  this->currentDuration = newDuration;
  this->sunsetDurationMs = DURATION_OPTIONS[this->currentDuration];
  this->registry.setInt("duration", this->currentDuration);
  
  Serial.println("[SunsetMode] Duration set to: " + DURATION_NAMES[this->currentDuration]);
  
  // Restart sunset with new duration if currently active
  if (this->sunsetActive) {
    this->startSunset();
  }
  
  return true;
}

CRGB SunsetMode::calculateSunsetColor(float progress) {
  if (progress < PHASE_BOUNDARIES[0]) {
    // Phase 1: Golden Hour (0-25%)
    float phaseProgress = progress / PHASE_BOUNDARIES[0];
    return lerpColor(PHASE_COLORS[0], PHASE_COLORS[1], phaseProgress);
  } else if (progress < PHASE_BOUNDARIES[1]) {
    // Phase 2: Orange Glow (25-50%)
    float phaseProgress = (progress - PHASE_BOUNDARIES[0]) / (PHASE_BOUNDARIES[1] - PHASE_BOUNDARIES[0]);
    return lerpColor(PHASE_COLORS[1], PHASE_COLORS[2], phaseProgress);
  } else if (progress < PHASE_BOUNDARIES[2]) {
    // Phase 3: Red Horizon (50-85%)
    float phaseProgress = (progress - PHASE_BOUNDARIES[1]) / (PHASE_BOUNDARIES[2] - PHASE_BOUNDARIES[1]);
    return lerpColor(PHASE_COLORS[2], PHASE_COLORS[3], phaseProgress);
  } else {
    // Phase 4: Twilight Fade (85-100%)
    float phaseProgress = (progress - PHASE_BOUNDARIES[2]) / (PHASE_BOUNDARIES[3] - PHASE_BOUNDARIES[2]);
    return lerpColor(PHASE_COLORS[3], PHASE_COLORS[4], phaseProgress);
  }
}

uint8_t SunsetMode::calculateBrightness(float progress) {
  // Logarithmic fade for natural perception
  float brightness = pow(1.0f - progress, 1.5f);
  return (uint8_t)(brightness * 255);
}

CRGB SunsetMode::lerpColor(CRGB color1, CRGB color2, float t) {
  // Clamp t between 0 and 1
  t = max(0.0f, min(1.0f, t));
  
  return CRGB(
    (uint8_t)(color1.r + t * (color2.r - color1.r)),
    (uint8_t)(color1.g + t * (color2.g - color1.g)),
    (uint8_t)(color1.b + t * (color2.b - color1.b))
  );
}

float SunsetMode::getSunsetProgress() {
  if (!this->sunsetActive) {
    return 1.0f; // Complete if not active
  }
  
  uint32_t elapsed = millis() - this->sunsetStartTime;
  return min(1.0f, (float)elapsed / (float)this->sunsetDurationMs);
}

SunsetMode::SunsetPhase SunsetMode::getCurrentPhase(float progress) {
  if (progress < PHASE_BOUNDARIES[0]) return GOLDEN_HOUR;
  if (progress < PHASE_BOUNDARIES[1]) return ORANGE_GLOW;
  if (progress < PHASE_BOUNDARIES[2]) return RED_HORIZON;
  if (progress < PHASE_BOUNDARIES[3]) return TWILIGHT_FADE;
  return COMPLETE;
}

void SunsetMode::startSunset(bool broadcast) {
  this->sunsetStartTime = millis();
  this->sunsetActive = true;
  this->currentPhase = GOLDEN_HOUR;
  this->registry.setBool("sunset_active", true);
  
  Serial.println("[SunsetMode] Starting " + DURATION_NAMES[this->currentDuration] + " sunset");
  
  if (broadcast) this->broadcastSunsetStart();
}

void SunsetMode::showDurationFeedback() {
  // Show current duration setting via brief blue flashes
  // 5min: 1 flash, 15min: 2 flashes, 30min: 3 flashes, 60min: 4 flashes
  
  // Brief blue color to indicate duration
  CRGB feedbackColor = CRGB::Blue;
  
  for (uint8_t flash = 0; flash <= this->currentDuration; flash++) {
    // Flash blue
    for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
      this->lightService->setLed(i, feedbackColor);
    }
    delay(200);
    
    // Turn off
    for (uint16_t i = 0; i < LED_NUM_LEDS; i++) {
      this->lightService->setLed(i, CRGB::Black);
    }
    delay(200);
  }
}

void SunsetMode::broadcastSunsetStart() {
  if (this->communicationService) {
    JsonDocument payload;
    payload["duration"] = this->currentDuration;
    
    this->sendCommand("start", payload);
    
    Serial.println("[SunsetMode] Broadcast sunset start");
  }
}

void SunsetMode::broadcastSunsetShutdown() {
  if (this->communicationService) {
    JsonDocument payload;
    payload.to<JsonObject>();
    
    this->sendCommand("shutdown", payload);
    
    Serial.println("[SunsetMode] Broadcast sunset shutdown");
  }
}
