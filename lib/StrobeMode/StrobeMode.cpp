#include "StrobeMode.h"

// Speed intervals: Slow (120 BPM), Medium (180 BPM), Fast (240 BPM), Ultra (360 BPM)
const uint32_t StrobeMode::SPEED_INTERVALS[4] = {
  500,  // 120 BPM
  333,  // 180 BPM
  250,  // 240 BPM
  167   // 360 BPM
};

const String StrobeMode::SPEED_NAMES[4] = {
  "Slow (120 BPM)",
  "Medium (180 BPM)",
  "Fast (240 BPM)", 
  "Ultra (360 BPM)"
};

// Party color palette
const CRGB StrobeMode::PARTY_COLORS[6] = {
  CRGB::HotPink,
  CRGB::DeepSkyBlue,
  CRGB::Lime,
  CRGB::Orange,
  CRGB::Magenta,
  CRGB::Cyan
};

StrobeMode::StrobeMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) 
  : AbstractMode(lightService, distanceService, communicationService) {
  this->title = "Strobe";
  this->description = "Synchronized party strobe lighting with mesh coordination";
  this->author = "Friedjof Noweck";
  this->contact = "programming@noweck.info";
  this->version = "1.0.0";
  this->license = "GPL-3.0";
}

void StrobeMode::setup() {
  // Initialize registry values
  this->registry.init("speed", RegistryType::INT, 1, 0, 3); // Default to Medium
  this->registry.init("pattern", RegistryType::INT, 0, 0, 3); // Default to White
  this->registry.init("emergency_stop", RegistryType::BOOL, false);

  // Load settings
  this->currentSpeed = this->registry.getInt("speed");
  this->currentPattern = this->registry.getInt("pattern");
  this->isEmergencyStop = this->registry.getBool("emergency_stop");

  // Initialize local effects
  this->intensityMultiplier = 1.0f;
  this->speedMultiplier = 1.0f;
  this->isBurstMode = false;
  this->isSoloMode = false;
  this->burstModeEnd = 0;
  this->soloModeEnd = 0;
  this->lastDistance = -1;
  this->lastGestureTime = 0;
  this->colorIndex = 0;
  
  // Initialize synchronization
  this->globalStartTime = 0;
  this->isSynchronized = false;

  // Set maximum brightness for strobe effect
  this->lightService->setBrightness(LED_MAX_BRIGHTNESS);

  // Add mode options
  this->addOption("Brightness", std::function<void()>([this](){ this->setBrightness(); }));
  this->addOption("Speed", std::function<void()>([this](){ this->newSpeed(); }));

  Serial.println("[StrobeMode] Setup complete - " + SPEED_NAMES[this->currentSpeed]);
  Serial.println("[StrobeMode] ⚠️  WARNING: Strobe lighting active - may cause seizures in epileptic individuals");
}

void StrobeMode::customFirst() {
  // Reset emergency stop when mode is activated
  this->isEmergencyStop = false;
  this->registry.setBool("emergency_stop", false);
  
  // Set a deterministic global start time for all lamps
  // Use a fixed reference point that all lamps can calculate
  uint32_t currentMeshTime = this->communicationService->getMeshTime();
  uint32_t interval = SPEED_INTERVALS[this->currentSpeed];
  
  // Synchronize to next 10-second boundary for perfect alignment
  this->globalStartTime = ((currentMeshTime / 10000) + 1) * 10000;
  this->isSynchronized = true;
  
  Serial.println("[StrobeMode] Activated - " + SPEED_NAMES[this->currentSpeed]);
  Serial.println("[StrobeMode] Pattern: " + String(this->currentPattern));
  Serial.println("[StrobeMode] Sync time: " + String(this->globalStartTime) + " (current: " + String(currentMeshTime) + ")");
}

void StrobeMode::customLoop() {
  // Emergency stop check
  if (this->isEmergencyStop) {
    this->lightService->fill(CRGB::Black);
    return;
  }

  // Update distance sensor effects
  this->updateDistanceSensorEffects();
  this->handleGestures();

  // Get synchronized mesh time
  uint32_t meshTime = this->communicationService->getMeshTime();

  // Apply local speed multiplier if active
  uint32_t effectiveInterval = SPEED_INTERVALS[this->currentSpeed];
  if (this->speedMultiplier != 1.0f) {
    effectiveInterval = (uint32_t)(effectiveInterval / this->speedMultiplier);
  }

  // Check for burst mode or solo mode
  if (this->isBurstMode && meshTime > this->burstModeEnd) {
    this->isBurstMode = false;
    this->speedMultiplier = 1.0f;
  }

  if (this->isSoloMode && meshTime > this->soloModeEnd) {
    this->isSoloMode = false;
  }

  // Solo mode: only this lamp strobes
  if (this->isSoloMode) {
    uint32_t soloInterval = 100; // Very fast solo strobe
    uint32_t timeInCycle = meshTime % soloInterval;
    if (timeInCycle < FLASH_DURATION) {
      CRGB color = this->getStrobeColor();
      color.nscale8((uint8_t)(255 * this->intensityMultiplier));
      this->lightService->fill(color);
    } else {
      this->lightService->fill(CRGB::Black);
    }
    return;
  }

  // Normal synchronized strobe
  if (this->shouldFlash(meshTime, effectiveInterval)) {
    CRGB color = this->getStrobeColor();
    
    // Apply intensity multiplier from distance sensor
    color.nscale8((uint8_t)(255 * this->intensityMultiplier));
    
    // Burst mode: extra bright
    if (this->isBurstMode) {
      color.nscale8(255); // Keep at maximum
    }
    
    this->lightService->fill(color);
  } else {
    this->lightService->fill(CRGB::Black);
  }
}

void StrobeMode::last() {
  // Save current state
  this->registry.setInt("speed", this->currentSpeed);
  this->registry.setInt("pattern", this->currentPattern);
  this->registry.setBool("emergency_stop", this->isEmergencyStop);
  
  // Turn off strobing
  this->lightService->fill(CRGB::Black);
}

void StrobeMode::customClick() {
  // Double click: Emergency stop
  this->isEmergencyStop = true;
  this->registry.setBool("emergency_stop", true);
  this->lightService->fill(CRGB::Black);
  
  Serial.println("[StrobeMode] 🚨 EMERGENCY STOP activated!");
  
  // Broadcast emergency stop to all lamps
  this->broadcastEmergencyStop();
}

bool StrobeMode::newSpeed() {
  if (!this->distanceService->isObjectPresent()) {
    return false;
  }

  // Use distance to select speed (4 levels)
  uint16_t level = this->getLevel();
  uint8_t newSpeed = map(level, 0, DISTANCE_LEVELS, 0, 3);
  
  if (newSpeed == this->currentSpeed) {
    return false;
  }

  this->currentSpeed = newSpeed;
  this->registry.setInt("speed", this->currentSpeed);
  
  // Re-synchronize with new speed - all lamps will do this calculation
  uint32_t currentMeshTime = this->communicationService->getMeshTime();
  this->globalStartTime = ((currentMeshTime / 5000) + 1) * 5000; // 5-second boundaries for speed changes
  
  Serial.println("[StrobeMode] Speed changed to: " + SPEED_NAMES[this->currentSpeed]);
  Serial.println("[StrobeMode] Re-sync time: " + String(this->globalStartTime));
  
  // Broadcast speed change to all lamps for UI feedback
  this->broadcastSpeedChange();
  
  return true;
}

bool StrobeMode::shouldFlash(uint32_t meshTime, uint32_t interval) {
  // If not synchronized yet or mesh time is before start time, don't flash
  if (!this->isSynchronized || meshTime < this->globalStartTime) {
    return false;
  }
  
  // Calculate time since synchronized start
  uint32_t timeSinceStart = meshTime - this->globalStartTime;
  
  // Add node-based offset for wave effect in color cycle mode only
  uint32_t offset = 0;
  if (this->currentPattern == COLOR_CYCLE) {
    uint32_t nodeId = this->communicationService->getNodeId();
    // Use last digit of nodeId for smaller, more predictable offset
    offset = (nodeId % 5) * (interval / 5); // Distribute across interval
  }
  
  uint32_t adjustedTime = timeSinceStart + offset;
  uint32_t timeInCycle = adjustedTime % interval;
  
  // Flash for FLASH_DURATION ms every interval
  bool shouldBeOn = timeInCycle < FLASH_DURATION;
  
  // Debug output every few seconds to check sync
  static uint32_t lastDebug = 0;
  if (meshTime - lastDebug > 2000) {
    Serial.printf("[StrobeMode] Node %u: time=%u, cycle=%u, on=%d\n", 
                  this->communicationService->getNodeId(), meshTime, timeInCycle, shouldBeOn);
    lastDebug = meshTime;
  }
  
  return shouldBeOn;
}

CRGB StrobeMode::getStrobeColor() {
  switch (this->currentPattern) {
    case WHITE_STROBE:
      return CRGB::White;
      
    case COLOR_CYCLE:
      return this->getColorCycleColor();
      
    case RANDOM_COLORS:
      return this->getRandomColor();
      
    case PARTY_PALETTE:
      return this->getPartyColor();
      
    default:
      return CRGB::White;
  }
}

void StrobeMode::updateDistanceSensorEffects() {
  if (!this->distanceService->isObjectPresent()) {
    // Reset multipliers when no hand detected
    this->intensityMultiplier = 1.0f;
    if (!this->isBurstMode) {
      this->speedMultiplier = 1.0f;
    }
    return;
  }

  uint16_t distance = this->distanceService->getDistance();
  
  // Intensity based on distance (closer = brighter)
  if (distance < 30) {
    this->intensityMultiplier = 1.5f; // 150% intensity
  } else if (distance < 60) {
    this->intensityMultiplier = 1.25f; // 125% intensity
  } else if (distance < 100) {
    this->intensityMultiplier = 1.0f; // Normal intensity
  } else {
    this->intensityMultiplier = 0.75f; // 75% intensity
  }

  // Speed boost when very close
  if (distance < 50 && !this->isBurstMode) {
    this->speedMultiplier = 1.5f; // 50% faster
  }
}

void StrobeMode::handleGestures() {
  if (!this->distanceService->isObjectPresent()) {
    this->lastDistance = -1;
    return;
  }

  uint16_t distance = this->distanceService->getDistance();
  uint32_t currentTime = this->communicationService->getMeshTime();
  
  // Detect quick hand movement (gesture)
  if (this->lastDistance != -1) {
    int distanceChange = abs(distance - this->lastDistance);
    
    // Quick movement detected
    if (distanceChange > 50 && (currentTime - this->lastGestureTime) > 1000) {
      // Trigger burst mode - 5 seconds of extra fast strobing
      this->isBurstMode = true;
      this->burstModeEnd = currentTime + 5000;
      this->speedMultiplier = 3.0f;
      this->lastGestureTime = currentTime;
      
      Serial.println("[StrobeMode] 💥 Burst mode activated by gesture!");
    }
  }
  
  // Long hand presence triggers solo mode
  if (distance < 40 && (currentTime - this->lastGestureTime) > 3000) {
    if (!this->isSoloMode) {
      this->isSoloMode = true;
      this->soloModeEnd = currentTime + 10000; // 10 seconds solo
      this->lastGestureTime = currentTime;
      
      Serial.println("[StrobeMode] ✨ Solo mode activated!");
    }
  }
  
  this->lastDistance = distance;
}

void StrobeMode::broadcastSpeedChange() {
  if (this->communicationService) {
    JsonDocument doc;
    doc["type"] = "strobe_speed_change";
    doc["speed"] = this->currentSpeed;
    doc["interval"] = SPEED_INTERVALS[this->currentSpeed];
    
    this->communicationService->sendEvent(doc);
    
    Serial.println("[StrobeMode] Broadcasted speed change");
  }
}

void StrobeMode::broadcastPatternChange() {
  if (this->communicationService) {
    JsonDocument doc;
    doc["type"] = "strobe_pattern_change";
    doc["pattern"] = this->currentPattern;
    
    this->communicationService->sendEvent(doc);
    
    Serial.println("[StrobeMode] Broadcasted pattern change");
  }
}

void StrobeMode::broadcastEmergencyStop() {
  if (this->communicationService) {
    JsonDocument doc;
    doc["type"] = "strobe_emergency_stop";
    doc["nodeId"] = this->communicationService->getNodeId();
    
    this->communicationService->sendEvent(doc);
    
    Serial.println("[StrobeMode] Broadcasted emergency stop");
  }
}

CRGB StrobeMode::getColorCycleColor() {
  // Rotate through rainbow colors
  uint32_t meshTime = this->communicationService->getMeshTime();
  uint8_t hue = (meshTime / 100) % 256; // Change color every 100ms
  return CHSV(hue, 255, 255);
}

CRGB StrobeMode::getRandomColor() {
  // Generate pseudo-random color based on mesh time
  uint32_t meshTime = this->communicationService->getMeshTime();
  uint32_t seed = meshTime / SPEED_INTERVALS[this->currentSpeed];
  
  // Simple PRNG
  seed = seed * 1103515245 + 12345;
  uint8_t r = (seed >> 16) % 256;
  
  seed = seed * 1103515245 + 12345;
  uint8_t g = (seed >> 16) % 256;
  
  seed = seed * 1103515245 + 12345;
  uint8_t b = (seed >> 16) % 256;
  
  return CRGB(r, g, b);
}

CRGB StrobeMode::getPartyColor() {
  // Cycle through party palette based on node ID for coordination
  uint32_t nodeId = this->communicationService->getNodeId();
  uint32_t meshTime = this->communicationService->getMeshTime();
  
  // Different nodes get different colors, but all change together
  uint8_t baseIndex = (meshTime / 1000) % 6; // Change every second
  uint8_t nodeOffset = nodeId % 6;
  uint8_t colorIndex = (baseIndex + nodeOffset) % 6;
  
  return PARTY_COLORS[colorIndex];
}

CRGB StrobeMode::getNodeBasedColor() {
  // Assign color based on node ID for coordinated multi-lamp effects
  uint32_t nodeId = this->communicationService->getNodeId();
  uint8_t colorIndex = nodeId % 6;
  return PARTY_COLORS[colorIndex];
}

void StrobeMode::synchronizeStrobeStart() {
  // Calculate next "round" start time for perfect synchronization
  uint32_t currentMeshTime = this->communicationService->getMeshTime();
  uint32_t interval = SPEED_INTERVALS[this->currentSpeed];
  
  // Round up to next interval boundary + 1 second for sync delay
  uint32_t syncDelay = 1000; // 1 second to allow all lamps to sync
  uint32_t nextBoundary = ((currentMeshTime / interval) + 1) * interval;
  this->globalStartTime = nextBoundary + syncDelay;
  
  // Broadcast sync start time to all lamps
  if (this->communicationService) {
    JsonDocument doc;
    doc["type"] = "strobe_sync_start";
    doc["start_time"] = this->globalStartTime;
    doc["speed"] = this->currentSpeed;
    doc["pattern"] = this->currentPattern;
    
    this->communicationService->sendEvent(doc);
    
    Serial.println("[StrobeMode] Broadcasted sync start time: " + String(this->globalStartTime));
  }
  
  this->isSynchronized = true;
}

void StrobeMode::handleMeshMessage(JsonDocument& message) {
  String type = message["type"];
  
  if (type == "strobe_sync_start") {
    // Synchronize start time with other lamps
    this->globalStartTime = message["start_time"];
    this->currentSpeed = message["speed"];
    this->currentPattern = message["pattern"];
    this->isSynchronized = true;
    
    Serial.println("[StrobeMode] Synchronized with start time: " + String(this->globalStartTime));
    
  } else if (type == "strobe_speed_change") {
    this->currentSpeed = message["speed"];
    this->registry.setInt("speed", this->currentSpeed);
    
    // Re-synchronize with new speed
    this->synchronizeStrobeStart();
    
    Serial.println("[StrobeMode] Speed synchronized: " + SPEED_NAMES[this->currentSpeed]);
    
  } else if (type == "strobe_pattern_change") {
    this->currentPattern = message["pattern"];
    this->registry.setInt("pattern", this->currentPattern);
    
    Serial.println("[StrobeMode] Pattern synchronized: " + String(this->currentPattern));
    
  } else if (type == "strobe_emergency_stop") {
    this->isEmergencyStop = true;
    this->registry.setBool("emergency_stop", true);
    this->lightService->fill(CRGB::Black);
    
    Serial.println("[StrobeMode] Emergency stop received from network");
  }
}
