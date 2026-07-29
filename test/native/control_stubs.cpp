#include "Alert.h"
#include "DistanceService.h"
#include "LightService.h"

LightService::LightService() = default;
void LightService::setHardwareBrightness(uint8_t brightness) {}
void LightService::setLightUpdateSteps(uint16_t steps) {}

result_t DistanceService::getResult() { return {0, LED_DEFAULT_BRIGHTNESS, 0}; }
uint16_t DistanceService::getNumberOfWipes() { return 0; }
void DistanceService::setNumberOfWipes(uint16_t numberOfWipes) {}
bool DistanceService::fixed() { return false; }
bool DistanceService::isObjectPresent() { return false; }
bool DistanceService::hasObjectDisappeared() { return false; }
bool DistanceService::hasWipeDetected() { return false; }
bool DistanceService::consumeLevelChange() { return false; }
bool DistanceService::alert() { return false; }

bool Alert::isFlashing() { return false; }
bool Alert::setFlashes(uint8_t flashes) { return true; }
bool Alert::setColor(CRGB color) { return true; }
