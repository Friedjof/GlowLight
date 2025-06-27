#include "RandomGlowMode.h"

// Optimized speed configurations: [pauseTime, transitionTime] in milliseconds
const uint32_t RandomGlowMode::SPEED_CONFIGS[4][2] = {
  {10000, 5000},  // Zen: Very slow (10s pause, 5s transition) - meditative
  {6000, 3000},   // Normal: Balanced (6s pause, 3s transition) - comfortable
  {3500, 1800},   // Lebendig: Active (3.5s pause, 1.8s transition) - dynamic
  {2000, 1000}    // Hektisch: Fast (2s pause, 1s transition) - energetic
};

const String RandomGlowMode::SPEED_NAMES[4] = {
  "Zen",
  "Normal", 
  "Lebendig",
  "Hektisch"
};

// 10 well-distributed HSV hues for rich color variety (every 36 degrees)
const uint16_t RandomGlowMode::COLOR_PALETTE[10] = {
  0,    // Red - Classic warm red
  36,   // Orange - Warm orange
  72,   // Yellow - Bright yellow
  108,  // Lime - Yellow-green
  144,  // Green - Pure green
  180,  // Cyan - Blue-green
  216,  // Light Blue - Sky blue
  252,  // Blue - Pure blue
  288,  // Purple - Blue-purple
  324   // Magenta - Red-purple
};

RandomGlowMode::RandomGlowMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) 
  : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Random Glow";
  this->description = "Simplified color flow using inherited brightness control - elegant pause and transition cycles";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "3.2.1";
  this->license = "GPL-3.0";
}

void RandomGlowMode::setup() {
  // Initialize registry values with optimized defaults
  this->registry.init("speed_mode", RegistryType::INT, 1, 0, 3); // Default to Normal
  this->registry.init("current_color", RegistryType::INT, 0, 0, 9); // Color palette index (0-9 for 10 colors)
  this->registry.init("next_color", RegistryType::INT, 1, 0, 9); // Next color index
  this->registry.init("distance_locked", RegistryType::BOOL, false);

  // Load settings
  this->currentSpeedMode = this->registry.getInt("speed_mode");
  this->currentColorIndex = this->registry.getInt("current_color");
  this->isDistanceLocked = this->registry.getBool("distance_locked");

  // Initialize state
  this->currentPhase = PAUSE;
  this->phaseStartTime = millis();
  this->lastDistanceCheck = 0;
  this->selectNextColor();
  this->startNewPhase();

  // Simple options using inherited brightness functionality
  this->addOption("Brightness", [this](){ this->setBrightness(); });
  this->addOption("Speed", [this](){ this->adjustSpeed(); });

  Serial.println("[RandomGlowMode] Setup complete - " + SPEED_NAMES[this->currentSpeedMode] + 
                 " | Brightness: " + String(this->brightness));
}

void RandomGlowMode::customFirst() {
  // Start with current color at static brightness
  this->currentPhase = PAUSE;
  this->startNewPhase();
  
  Serial.println("[RandomGlowMode] Started - Color: " + String(COLOR_PALETTE[this->currentColorIndex]) + "°");
}

void RandomGlowMode::customLoop() {
  uint32_t currentTime = millis();
  
  // Optimized distance effects check (every 100ms)
  this->updateDistanceEffects();
  
  // Check phase transitions
  if (currentTime - this->phaseStartTime >= this->phaseDuration) {
    if (this->currentPhase == PAUSE) {
      // Start transition to next color
      this->selectNextColor();
      this->currentPhase = TRANSITION;
      this->startNewPhase();
      Serial.println("[RandomGlowMode] → " + String(COLOR_PALETTE[this->nextColorIndex]) + "°");
    } else {
      // Complete transition - switch to new color
      this->currentColorIndex = this->nextColorIndex;
      this->registry.setInt("current_color", this->currentColorIndex);
      this->currentPhase = PAUSE;
      this->startNewPhase();
      Serial.println("[RandomGlowMode] ⏸ " + String(COLOR_PALETTE[this->currentColorIndex]) + "°");
    }
  }
  
  // Update lighting (this is called every loop for smooth transitions)
  this->updateLighting();
}

void RandomGlowMode::customStop() {
  Serial.println("[RandomGlowMode] Stopping");
}

void RandomGlowMode::last() {
  // Save state
  this->registry.setInt("speed_mode", this->currentSpeedMode);
  this->registry.setInt("current_color", this->currentColorIndex);
  
  Serial.println("[RandomGlowMode] State saved");
}

void RandomGlowMode::customClick() {
  // Toggle distance sensor lock with improved feedback
  this->isDistanceLocked = !this->isDistanceLocked;
  this->registry.setBool("distance_locked", this->isDistanceLocked);
  this->broadcastSettingChange("distance_locked", this->isDistanceLocked);
  
  // Clear visual feedback with lock status
  this->lightService->fill(this->isDistanceLocked ? CRGB::Red : CRGB::Green);
  delay(200);
  this->lightService->fill(CRGB::Black);
  delay(100);
  this->lightService->fill(this->isDistanceLocked ? CRGB::Red : CRGB::Green);
  delay(200);
  
  Serial.println("[RandomGlowMode] 🔒 Distance control " + 
                 String(this->isDistanceLocked ? "LOCKED" : "UNLOCKED"));
}

bool RandomGlowMode::newSpeed() {
  this->currentSpeedMode = (this->currentSpeedMode + 1) % 4;
  this->registry.setInt("speed_mode", this->currentSpeedMode);
  
  // Restart current phase with new timing
  this->startNewPhase();
  
  Serial.println("[RandomGlowMode] Speed: " + SPEED_NAMES[this->currentSpeedMode]);
  return true;
}

void RandomGlowMode::startNewPhase() {
  this->phaseStartTime = millis();
  
  if (this->currentPhase == PAUSE) {
    // Random pause duration with ±20% variation
    uint32_t baseTime = SPEED_CONFIGS[this->currentSpeedMode][0];
    this->phaseDuration = this->getRandomDuration(baseTime);
  } else {
    // Transition duration with ±20% variation
    uint32_t baseTime = SPEED_CONFIGS[this->currentSpeedMode][1];
    this->phaseDuration = this->getRandomDuration(baseTime);
  }
}

void RandomGlowMode::selectNextColor() {
  // Optimized color selection: avoid current color and favor distant hues
  uint8_t attempts = 0;
  do {
    this->nextColorIndex = random(0, 10);
    attempts++;
  } while (this->nextColorIndex == this->currentColorIndex && attempts < 10);
  
  this->registry.setInt("next_color", this->nextColorIndex);
}

void RandomGlowMode::updateLighting() {
  CRGB targetColor;
  
  if (this->currentPhase == PAUSE) {
    // Static color during pause
    targetColor = CHSV(COLOR_PALETTE[this->currentColorIndex], 255, 255);
  } else {
    // Smooth transition using optimized interpolation
    uint32_t elapsed = millis() - this->phaseStartTime;
    float progress = min(1.0f, (float)elapsed / (float)this->phaseDuration);
    
    // Use FastLED's blend function for smoother color mixing
    CRGB currentColor = CHSV(COLOR_PALETTE[this->currentColorIndex], 255, 255);
    CRGB nextColor = CHSV(COLOR_PALETTE[this->nextColorIndex], 255, 255);
    targetColor = blend(currentColor, nextColor, (uint8_t)(progress * 255));
  }
  
  // Apply brightness and display
  targetColor.nscale8(this->brightness);
  this->lightService->fill(targetColor);
}

void RandomGlowMode::updateDistanceEffects() {
  uint32_t currentTime = millis();
  
  // Throttle to every 100ms for efficiency
  if (currentTime - this->lastDistanceCheck < 100) {
    return;
  }
  this->lastDistanceCheck = currentTime;
  
  // Skip if locked or no object present
  if (this->isDistanceLocked || !this->distanceService->isObjectPresent()) {
    return;
  }
  
  uint16_t level = this->getLevel();
  
  // Speed control only (brightness is handled by AbstractMode)
  if (level > 50) {
    uint8_t newSpeedMode;
    if (level >= 85) newSpeedMode = 3;      // Hektisch (85-100%)
    else if (level >= 70) newSpeedMode = 2;  // Lebendig (70-84%)
    else if (level >= 55) newSpeedMode = 1;  // Normal (55-69%)
    else newSpeedMode = 0;                   // Zen (51-54%)
    
    if (newSpeedMode != this->currentSpeedMode) {
      this->currentSpeedMode = newSpeedMode;
      this->registry.setInt("speed_mode", newSpeedMode);
      this->startNewPhase(); // Apply new timing immediately
      this->broadcastSettingChange("speed_mode", newSpeedMode);
      Serial.println("[RandomGlowMode] ⚡ " + SPEED_NAMES[newSpeedMode]);
    }
  }
}

void RandomGlowMode::adjustSpeed() {
  if (!this->distanceService->isObjectPresent() || this->isDistanceLocked) return;
  
  uint16_t level = this->getLevel();
  uint8_t newSpeedMode = (level >= 75) ? 3 : (level >= 50) ? 2 : (level >= 25) ? 1 : 0;
  
  if (newSpeedMode != this->currentSpeedMode) {
    this->currentSpeedMode = newSpeedMode;
    this->registry.setInt("speed_mode", newSpeedMode);
    this->startNewPhase();
    this->broadcastSettingChange("speed_mode", newSpeedMode);
  }
}

uint32_t RandomGlowMode::getRandomDuration(uint32_t baseTime) {
  // Optimized random variation: ±25% for more natural feeling
  uint32_t variation = baseTime >> 2; // Bit shift for /4 (25%)
  return baseTime + random(0, variation * 2 + 1) - variation;
}

void RandomGlowMode::broadcastSettingChange(String key, int value) {
  JsonDocument message;
  message["type"] = "random_glow_setting";
  message["key"] = key;
  message["value"] = value;
  message["nodeId"] = this->communicationService->getNodeId();
  
  this->communicationService->sendEvent(message);
}

void RandomGlowMode::broadcastSettingChange(String key, bool value) {
  JsonDocument message;
  message["type"] = "random_glow_setting";
  message["key"] = key;
  message["value"] = value;
  message["nodeId"] = this->communicationService->getNodeId();
  
  this->communicationService->sendEvent(message);
}
