// Host shim for ESP-NOW. Captures every transmitted frame and exposes the
// registered callbacks so tests can inject raw radio traffic.
#ifndef GLOW_SHIM_ESP_NOW_H
#define GLOW_SHIM_ESP_NOW_H

#include <cstddef>
#include <cstdint>
#include <vector>

#define ESP_NOW_MAX_DATA_LEN 250
#define ESP_NOW_ETH_ALEN 6
#define ESP_NOW_KEY_LEN 16

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_ESPNOW_BASE (-100)

typedef enum {
  ESP_NOW_SEND_SUCCESS = 0,
  ESP_NOW_SEND_FAIL,
} esp_now_send_status_t;

typedef struct {
  uint8_t peer_addr[ESP_NOW_ETH_ALEN];
  uint8_t lmk[ESP_NOW_KEY_LEN];
  uint8_t channel;
  int ifidx;
  bool encrypt;
  void* priv;
} esp_now_peer_info_t;

typedef void (*esp_now_recv_cb_t)(const uint8_t* mac_addr, const uint8_t* data, int len);
typedef void (*esp_now_send_cb_t)(const uint8_t* mac_addr, esp_now_send_status_t status);

esp_err_t esp_now_init(void);
esp_err_t esp_now_deinit(void);
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb);
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb);
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_send(const uint8_t* peer_addr, const uint8_t* data, size_t len);

namespace glow_shim {

struct CapturedFrame {
  std::vector<uint8_t> data;
  uint8_t channel;
};

// Every frame handed to esp_now_send(), in order.
extern std::vector<CapturedFrame> sentFrames;
// When true, esp_now_send() reports ESP_FAIL without capturing.
extern bool sendShouldFail;
// When true, the send-complete callback is never invoked (models a lost ack).
extern bool suppressSendCallback;
extern uint8_t addedPeerChannel;
extern int addPeerCalls;
extern bool addPeerShouldFail;

extern esp_now_recv_cb_t recvCallback;
extern esp_now_send_cb_t sendCallback;

void resetEspNow();
// Feeds a raw frame into the driver callback, exactly like the radio would.
void deliverFrame(const uint8_t* mac, const uint8_t* data, size_t length);

}  // namespace glow_shim

#endif
