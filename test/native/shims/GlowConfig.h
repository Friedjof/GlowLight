// Configuration used by the native test build. Mirrors the production
// GlowConfig.h but only defines what CommunicationService reads.
#ifndef GLOW_SHIM_GLOWCONFIG_H
#define GLOW_SHIM_GLOWCONFIG_H

#define MESH_ON true
#define ESPNOW_CHANNEL 1

// Fixed test group key so frames are reproducible. Never use on a device.
#define GLOW_GROUP_KEY_HEX "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
#define GLOW_MAX_GROUP_NODES 8

#define GLOW_NODE_TIMEOUT 30 * 60 * 1000
#define HARTBEAT_INTERVAL 10000
#define LEVEL_UPDATE_INTERVAL 100

#define LED_NUM_LEDS 11
#define LED_MAX_BRIGHTNESS 255
#define LED_MIN_BRIGHTNESS 0
#define LED_DEFAULT_BRIGHTNESS 128
#define LED_UPDATE_STEPS 20
#define DISTANCE_MAX_MM 200
#define DISTANCE_LEVELS 255
#define ALERT_NUM_FLASHES 5
#define ALERT_SPEED_STEP 32

#endif
