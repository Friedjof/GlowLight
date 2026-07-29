// Implementation of the host shims: clock, serial capture, RNG, ESP-NOW
// capture, FreeRTOS queues and the mbedTLS subset mapped onto OpenSSL.

#include <openssl/evp.h>

#include <cstring>
#include <deque>
#include <map>
#include <vector>

#include "Arduino.h"
#include "ESPmDNS.h"
#include "WiFi.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"

// ---------------------------------------------------------------- Arduino ---

namespace glow_shim {
uint32_t clockMillis = 0;
bool serialEcho = false;
std::vector<std::string> serialLog;
uint8_t localMac[6] = {0x98, 0x3d, 0xae, 0x52, 0x87, 0x7c};
std::map<std::string, std::string> preferencesValues;
bool preferencesBeginShouldFail = false;
bool preferencesPutShouldFail = false;

void resetPreferences() {
  preferencesValues.clear();
  preferencesBeginShouldFail = false;
  preferencesPutShouldFail = false;
}

int wifiStatus = WL_DISCONNECTED;
bool wifiSleep = true;
int wifiMode = WIFI_OFF;
int wifiBeginCalls = 0;
int wifiBeginChannel = 0;
int wifiDisconnectCalls = 0;
std::string wifiSsid;
std::string wifiPassword;
std::string wifiHostname;
int wifiSoftApCalls = 0;
int wifiSoftApChannel = 0;
bool wifiSoftApShouldFail = false;
std::string wifiSoftApSsid;
std::string wifiSoftApPassword;

uint8_t radioChannel = 0;
int channelSetCalls = 0;
esp_err_t channelSetResult = ESP_OK;
esp_err_t channelGetResult = ESP_OK;

std::string mdnsHostname;
int mdnsBeginCalls = 0;
int mdnsEndCalls = 0;
bool mdnsShouldFail = false;

void resetNetwork() {
  wifiStatus = WL_DISCONNECTED;
  wifiSleep = true;
  wifiMode = WIFI_OFF;
  wifiBeginCalls = 0;
  wifiBeginChannel = 0;
  wifiDisconnectCalls = 0;
  wifiSsid.clear();
  wifiPassword.clear();
  wifiHostname.clear();
  wifiSoftApCalls = 0;
  wifiSoftApChannel = 0;
  wifiSoftApShouldFail = false;
  wifiSoftApSsid.clear();
  wifiSoftApPassword.clear();
  radioChannel = 0;
  channelSetCalls = 0;
  channelSetResult = ESP_OK;
  channelGetResult = ESP_OK;
  mdnsHostname.clear();
  mdnsBeginCalls = 0;
  mdnsEndCalls = 0;
  mdnsShouldFail = false;
}
}  // namespace glow_shim

SerialClass Serial;
WiFiClass WiFi;
MDNSStub MDNS;

esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t) {
  ++glow_shim::channelSetCalls;
  if (glow_shim::channelSetResult != ESP_OK) return glow_shim::channelSetResult;
  glow_shim::radioChannel = primary;
  return ESP_OK;
}

esp_err_t esp_wifi_get_channel(uint8_t* primary, wifi_second_chan_t* second) {
  if (glow_shim::channelGetResult != ESP_OK) return glow_shim::channelGetResult;
  if (primary != nullptr) *primary = glow_shim::radioChannel;
  if (second != nullptr) *second = WIFI_SECOND_CHAN_NONE;
  return ESP_OK;
}

// ------------------------------------------------------------------- RNG ----

namespace {
uint64_t randomState = 0x0123456789abcdefULL;

uint64_t nextRandom() {
  // xorshift64*, deterministic and good enough for test material.
  randomState ^= randomState >> 12;
  randomState ^= randomState << 25;
  randomState ^= randomState >> 27;
  return randomState * 0x2545f4914f6cdd1dULL;
}
}  // namespace

namespace glow_shim {
void seedRandom(uint64_t seed) { randomState = seed != 0 ? seed : 0x0123456789abcdefULL; }
}  // namespace glow_shim

void esp_fill_random(void* buffer, size_t length) {
  uint8_t* bytes = static_cast<uint8_t*>(buffer);
  for (size_t i = 0; i < length; ++i) bytes[i] = static_cast<uint8_t>(nextRandom() >> 24);
}

uint32_t esp_random(void) { return static_cast<uint32_t>(nextRandom() >> 16); }

// --------------------------------------------------------------- ESP-NOW ----

namespace glow_shim {
std::vector<CapturedFrame> sentFrames;
bool sendShouldFail = false;
bool suppressSendCallback = false;
uint8_t addedPeerChannel = 255;
int addPeerCalls = 0;
bool addPeerShouldFail = false;
esp_now_recv_cb_t recvCallback = nullptr;
esp_now_send_cb_t sendCallback = nullptr;

void resetEspNow() {
  sentFrames.clear();
  sendShouldFail = false;
  suppressSendCallback = false;
  addedPeerChannel = 255;
  addPeerCalls = 0;
  addPeerShouldFail = false;
  recvCallback = nullptr;
  sendCallback = nullptr;
}

void deliverFrame(const uint8_t* mac, const uint8_t* data, size_t length) {
  if (recvCallback != nullptr) recvCallback(mac, data, static_cast<int>(length));
}
}  // namespace glow_shim

esp_err_t esp_now_init(void) { return ESP_OK; }

esp_err_t esp_now_deinit(void) {
  glow_shim::recvCallback = nullptr;
  glow_shim::sendCallback = nullptr;
  return ESP_OK;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb) {
  glow_shim::recvCallback = cb;
  return ESP_OK;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb) {
  glow_shim::sendCallback = cb;
  return ESP_OK;
}

esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
  ++glow_shim::addPeerCalls;
  if (glow_shim::addPeerShouldFail) return ESP_FAIL;
  glow_shim::addedPeerChannel = peer != nullptr ? peer->channel : 255;
  return ESP_OK;
}

esp_err_t esp_now_send(const uint8_t* peerAddress, const uint8_t* data, size_t length) {
  if (glow_shim::sendShouldFail) return ESP_FAIL;

  glow_shim::CapturedFrame frame;
  frame.data.assign(data, data + length);
  frame.channel = glow_shim::radioChannel;
  glow_shim::sentFrames.push_back(frame);

  if (!glow_shim::suppressSendCallback && glow_shim::sendCallback != nullptr)
    glow_shim::sendCallback(peerAddress, ESP_NOW_SEND_SUCCESS);
  return ESP_OK;
}

// -------------------------------------------------------------- Queues ------

namespace glow_shim {
int queueCreateFailuresRemaining = 0;
}  // namespace glow_shim

struct GlowShimQueue {
  size_t itemSize;
  size_t capacity;
  std::deque<std::vector<uint8_t>> items;
};

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
  if (glow_shim::queueCreateFailuresRemaining > 0) {
    --glow_shim::queueCreateFailuresRemaining;
    return nullptr;
  }
  GlowShimQueue* queue = new GlowShimQueue();
  queue->itemSize = itemSize;
  queue->capacity = length;
  return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t) {
  if (queue == nullptr || queue->items.size() >= queue->capacity) return pdFALSE;
  const uint8_t* bytes = static_cast<const uint8_t*>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t) {
  if (queue == nullptr || queue->items.empty()) return pdFALSE;
  memcpy(buffer, queue->items.front().data(), queue->itemSize);
  queue->items.pop_front();
  return pdTRUE;
}

UBaseType_t uxQueueSpacesAvailable(QueueHandle_t queue) {
  if (queue == nullptr) return 0;
  return static_cast<UBaseType_t>(queue->capacity - queue->items.size());
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) {
  if (queue == nullptr) return 0;
  return static_cast<UBaseType_t>(queue->items.size());
}

void vQueueDelete(QueueHandle_t queue) { delete queue; }

// -------------------------------------------------------------- mbedTLS -----

namespace {

const mbedtls_md_info_t SHA256_INFO = {MBEDTLS_MD_SHA256, 32};

bool sha256(const uint8_t* input, size_t inputLength, uint8_t* output) {
  unsigned int length = 0;
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  if (context == nullptr) return false;
  bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
            EVP_DigestUpdate(context, input, inputLength) == 1 &&
            EVP_DigestFinal_ex(context, output, &length) == 1;
  EVP_MD_CTX_free(context);
  return ok && length == 32;
}

// Straightforward HMAC-SHA256; the messages involved here are a few dozen bytes.
bool hmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* message,
                size_t messageLength, uint8_t* output) {
  uint8_t normalizedKey[64] = {};
  if (keyLength > 64) {
    if (!sha256(key, keyLength, normalizedKey)) return false;
  } else {
    memcpy(normalizedKey, key, keyLength);
  }

  std::vector<uint8_t> inner(64 + messageLength);
  for (size_t i = 0; i < 64; ++i) inner[i] = normalizedKey[i] ^ 0x36;
  if (messageLength > 0) memcpy(inner.data() + 64, message, messageLength);

  uint8_t innerDigest[32];
  if (!sha256(inner.data(), inner.size(), innerDigest)) return false;

  uint8_t outer[64 + 32];
  for (size_t i = 0; i < 64; ++i) outer[i] = normalizedKey[i] ^ 0x5c;
  memcpy(outer + 64, innerDigest, sizeof(innerDigest));
  return sha256(outer, sizeof(outer), output);
}

}  // namespace

const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t type) {
  return type == MBEDTLS_MD_SHA256 ? &SHA256_INFO : nullptr;
}

unsigned char mbedtls_md_get_size(const mbedtls_md_info_t* info) {
  return info == nullptr ? 0 : static_cast<unsigned char>(info->size);
}

void mbedtls_md_init(mbedtls_md_context_t* context) {
  context->info = nullptr;
  context->hmac = false;
  context->started = false;
  context->key.clear();
  context->message.clear();
}

void mbedtls_md_free(mbedtls_md_context_t* context) { mbedtls_md_init(context); }

int mbedtls_md_setup(mbedtls_md_context_t* context, const mbedtls_md_info_t* info, int hmac) {
  if (info == nullptr) return -1;
  context->info = info;
  context->hmac = hmac != 0;
  return 0;
}

int mbedtls_md_hmac(const mbedtls_md_info_t* info, const unsigned char* key, size_t keyLength,
                    const unsigned char* input, size_t inputLength, unsigned char* output) {
  if (info == nullptr || info->type != MBEDTLS_MD_SHA256) return -1;
  return hmacSha256(key, keyLength, input, inputLength, output) ? 0 : -1;
}

int mbedtls_md_hmac_starts(mbedtls_md_context_t* context, const unsigned char* key,
                           size_t keyLength) {
  if (context->info == nullptr || !context->hmac) return -1;
  context->key.assign(reinterpret_cast<const char*>(key), keyLength);
  context->message.clear();
  context->started = true;
  return 0;
}

int mbedtls_md_hmac_update(mbedtls_md_context_t* context, const unsigned char* input,
                           size_t inputLength) {
  if (!context->started) return -1;
  context->message.append(reinterpret_cast<const char*>(input), inputLength);
  return 0;
}

int mbedtls_md_hmac_finish(mbedtls_md_context_t* context, unsigned char* output) {
  if (!context->started) return -1;
  bool ok = hmacSha256(reinterpret_cast<const uint8_t*>(context->key.data()),
                       context->key.size(),
                       reinterpret_cast<const uint8_t*>(context->message.data()),
                       context->message.size(), output);
  context->message.clear();
  return ok ? 0 : -1;
}

void mbedtls_gcm_init(mbedtls_gcm_context* context) {
  memset(context->key, 0, sizeof(context->key));
  context->keyLength = 0;
  context->ready = false;
}

void mbedtls_gcm_free(mbedtls_gcm_context* context) { mbedtls_gcm_init(context); }

int mbedtls_gcm_setkey(mbedtls_gcm_context* context, mbedtls_cipher_id_t cipher,
                       const unsigned char* key, unsigned int keyBits) {
  if (cipher != MBEDTLS_CIPHER_ID_AES || keyBits != 256) return MBEDTLS_ERR_GCM_BAD_INPUT;
  memcpy(context->key, key, 32);
  context->keyLength = 32;
  context->ready = true;
  return 0;
}

int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* context, int mode, size_t length,
                              const unsigned char* iv, size_t ivLength,
                              const unsigned char* add, size_t addLength,
                              const unsigned char* input, unsigned char* output,
                              size_t tagLength, unsigned char* tag) {
  if (!context->ready || mode != MBEDTLS_GCM_ENCRYPT) return MBEDTLS_ERR_GCM_BAD_INPUT;

  EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
  if (cipher == nullptr) return MBEDTLS_ERR_GCM_BAD_INPUT;

  int outputLength = 0;
  bool ok = EVP_EncryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(ivLength), nullptr) == 1 &&
            EVP_EncryptInit_ex(cipher, nullptr, nullptr, context->key, iv) == 1;
  if (ok && addLength > 0)
    ok = EVP_EncryptUpdate(cipher, nullptr, &outputLength, add,
                           static_cast<int>(addLength)) == 1;
  if (ok && length > 0)
    ok = EVP_EncryptUpdate(cipher, output, &outputLength, input,
                           static_cast<int>(length)) == 1;
  if (ok) {
    int finalLength = 0;
    ok = EVP_EncryptFinal_ex(cipher, output + outputLength, &finalLength) == 1 &&
         EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tagLength),
                             tag) == 1;
  }
  EVP_CIPHER_CTX_free(cipher);
  return ok ? 0 : MBEDTLS_ERR_GCM_BAD_INPUT;
}

int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* context, size_t length,
                             const unsigned char* iv, size_t ivLength,
                             const unsigned char* add, size_t addLength,
                             const unsigned char* tag, size_t tagLength,
                             const unsigned char* input, unsigned char* output) {
  if (!context->ready) return MBEDTLS_ERR_GCM_BAD_INPUT;

  EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
  if (cipher == nullptr) return MBEDTLS_ERR_GCM_BAD_INPUT;

  int outputLength = 0;
  bool ok = EVP_DecryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(ivLength), nullptr) == 1 &&
            EVP_DecryptInit_ex(cipher, nullptr, nullptr, context->key, iv) == 1;
  if (ok && addLength > 0)
    ok = EVP_DecryptUpdate(cipher, nullptr, &outputLength, add,
                           static_cast<int>(addLength)) == 1;
  if (ok && length > 0)
    ok = EVP_DecryptUpdate(cipher, output, &outputLength, input,
                           static_cast<int>(length)) == 1;
  if (ok)
    ok = EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tagLength),
                             const_cast<unsigned char*>(tag)) == 1;
  if (ok) {
    int finalLength = 0;
    ok = EVP_DecryptFinal_ex(cipher, output + outputLength, &finalLength) == 1;
  }
  EVP_CIPHER_CTX_free(cipher);
  return ok ? 0 : MBEDTLS_ERR_GCM_AUTH_FAILED;
}
