#include "ModeRegistration.h"

#include "ModeConfig.h"

#include "Controller.h"
#include "LightService.h"
#include "DistanceService.h"
#include "CommunicationService.h"

#include "Alert.h"
#include "StaticMode.h"
#include "ColorPickerMode.h"
#include "RainbowMode.h"
#include "RandomGlowMode.h"
#include "BeaconMode.h"
#include "CandleMode.h"
#include "SunsetMode.h"
#include "StrobeMode.h"
#include "MiniGame.h"

void registerModes(Controller& controller, LightService& lightService,
                   DistanceService& distanceService,
                   CommunicationService& communicationService) {
#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_STATIC_MODE
  static StaticMode staticMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&staticMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_COLOR_PICKER_MODE
  static ColorPickerMode colorPickerMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&colorPickerMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_RAINBOW_MODE
  static RainbowMode rainbowMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&rainbowMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_RANDOM_GLOW_MODE
  static RandomGlowMode randomGlowMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&randomGlowMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_BEACON_MODE
  static BeaconMode beaconMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&beaconMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_CANDLE_MODE
  static CandleMode candleMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&candleMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_SUNSET_MODE
  static SunsetMode sunsetMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&sunsetMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_STROBE_MODE
  static StrobeMode strobeMode(&lightService, &distanceService, &communicationService);
  controller.addMode(&strobeMode);
#endif

#if GLOW_ENABLE_ALL_MODES || GLOW_ENABLE_MINI_GAME_MODE
  static MiniGame miniGame(&lightService, &distanceService, &communicationService);
  controller.addMode(&miniGame);
#endif

  static Alert alertMode(&lightService, &distanceService, &communicationService);
  controller.setAlertMode(&alertMode);
}
