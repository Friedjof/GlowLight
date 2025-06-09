#ifndef STROBEMODE_H
#define STROBEMODE_H

#include <Arduino.h>
#include <FastLED.h>

#include "AbstractMode.h"

class StrobeMode : public AbstractMode {
  public:
    StrobeMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newSpeed();

  private:
    // Strobe settings (shared across all lamps)
    static const uint32_t SPEED_INTERVALS[4];
    static const String SPEED_NAMES[4];
    static const uint32_t FLASH_DURATION = 50; // Flash on time in ms

    // Pattern definitions
    enum StrobePattern {
      WHITE_STROBE = 0,
      COLOR_CYCLE = 1,
      RANDOM_COLORS = 2,
      PARTY_PALETTE = 3
    };

    // Party color palette
    static const CRGB PARTY_COLORS[6];

    // Current settings
    uint8_t currentSpeed;
    uint8_t currentPattern;
    bool isEmergencyStop;
    
    // Synchronization
    uint32_t globalStartTime;
    bool isSynchronized;

    // Distance sensor local effects
    float intensityMultiplier;
    float speedMultiplier;
    uint32_t burstModeEnd;
    bool isBurstMode;
    bool isSoloMode;
    uint32_t soloModeEnd;

    // Gesture detection
    int lastDistance;
    uint32_t lastGestureTime;

    // Color cycling
    uint8_t colorIndex;

    // Helper methods
    bool shouldFlash(uint32_t meshTime, uint32_t interval);
    void synchronizeStrobeStart();
    void handleMeshMessage(JsonDocument& message);
    CRGB getStrobeColor();
    void updateDistanceSensorEffects();
    void handleGestures();
    void broadcastSpeedChange();
    void broadcastPatternChange();
    void broadcastEmergencyStop();
    
    // Color helpers
    CRGB getColorCycleColor();
    CRGB getRandomColor();
    CRGB getPartyColor();
    CRGB getNodeBasedColor();
};

#endif
