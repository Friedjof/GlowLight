#ifndef RANDOMGLOWMODE_H
#define RANDOMGLOWMODE_H

#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#include "AbstractMode.h"

class RandomGlowMode : public AbstractMode {
  public:
    RandomGlowMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void customStop();
    void last();

    void customClick();

    bool newSpeed();

  private:
    // Simplified phase states - only pause and transition
    enum GlowPhase {
      PAUSE = 0,         // Static color at set brightness
      TRANSITION = 1     // Smooth transition to next color
    };

    // Speed mode definitions (4 levels from slow to fast)
    static const uint32_t SPEED_CONFIGS[4][2]; // [mode][pauseTime, transitionTime]
    static const String SPEED_NAMES[4];

    // Simplified color palette (6 HSV hues for harmonious variety)
    static const uint16_t COLOR_PALETTE[10];

    // Current state
    GlowPhase currentPhase;
    uint8_t currentSpeedMode;    // 0-3: Zen/Normal/Lebendig/Hektisch
    uint8_t currentColorIndex;
    uint8_t nextColorIndex;
    
    // Timing
    uint32_t phaseStartTime;
    uint32_t phaseDuration;
    
    // Distance sensor
    bool isDistanceLocked;
    uint32_t lastDistanceCheck;

    // Helper methods
    void startNewPhase();
    void selectNextColor();
    void updateLighting();
    void updateDistanceEffects();
    void adjustSpeed();
    uint32_t getRandomDuration(uint32_t baseTime);
    
    // Settings broadcast
    void broadcastSettingChange(String key, int value);
    void broadcastSettingChange(String key, bool value);
};

#endif
