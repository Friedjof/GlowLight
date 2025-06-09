#ifndef SUNSETMODE_H
#define SUNSETMODE_H

#include <Arduino.h>
#include <FastLED.h>

#include "AbstractMode.h"

class SunsetMode : public AbstractMode {
  public:
    SunsetMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

    void setup();

    void customFirst();
    void customLoop();
    void last();

    void customClick();

    bool newDuration();

  private:
    // Sunset phases
    enum SunsetPhase {
      GOLDEN_HOUR = 0,    // 0-25%
      ORANGE_GLOW = 1,    // 25-50%
      RED_HORIZON = 2,    // 50-85%
      TWILIGHT_FADE = 3,  // 85-100%
      COMPLETE = 4        // Finished
    };

    // Duration options (in milliseconds)
    static const uint32_t DURATION_OPTIONS[4];
    static const String DURATION_NAMES[4];

    // Color definitions for phases
    static const CRGB PHASE_COLORS[5];

    // Phase boundaries
    static const float PHASE_BOUNDARIES[4];

    // Current state
    uint32_t sunsetStartTime;
    uint32_t sunsetDurationMs;
    uint8_t currentDuration;
    SunsetPhase currentPhase;
    bool isManualShutdown;
    bool sunsetActive;

    // Helper methods
    CRGB calculateSunsetColor(float progress);
    uint8_t calculateBrightness(float progress);
    CRGB lerpColor(CRGB color1, CRGB color2, float t);
    float getSunsetProgress();
    SunsetPhase getCurrentPhase(float progress);
    void startSunset();
    void showDurationFeedback();
    void broadcastSunsetStart();
    void broadcastSunsetShutdown();
};

#endif
