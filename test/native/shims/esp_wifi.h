// Host shim for the few esp_wifi calls the NetworkService makes.
#ifndef GLOW_SHIM_ESP_WIFI_H
#define GLOW_SHIM_ESP_WIFI_H

#include <cstdint>

#include "esp_now.h"  // esp_err_t, ESP_OK

typedef enum {
  WIFI_SECOND_CHAN_NONE = 0,
  WIFI_SECOND_CHAN_ABOVE,
  WIFI_SECOND_CHAN_BELOW,
} wifi_second_chan_t;

esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t esp_wifi_get_channel(uint8_t* primary, wifi_second_chan_t* second);

namespace glow_shim {
// Channel the radio currently sits on, as seen by esp_wifi_get_channel().
extern uint8_t radioChannel;
extern int channelSetCalls;
extern esp_err_t channelSetResult;
extern esp_err_t channelGetResult;

// Resets WiFi, channel and mDNS state between tests.
void resetNetwork();
}  // namespace glow_shim

#endif
