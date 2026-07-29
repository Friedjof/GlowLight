// Host shim for the mbedTLS GCM API, backed by OpenSSL's AES-GCM.
#ifndef GLOW_SHIM_MBEDTLS_GCM_H
#define GLOW_SHIM_MBEDTLS_GCM_H

#include <cstddef>
#include <cstdint>

#define MBEDTLS_ERR_GCM_AUTH_FAILED (-0x0012)
#define MBEDTLS_ERR_GCM_BAD_INPUT (-0x0014)

typedef enum {
  MBEDTLS_CIPHER_ID_NONE = 0,
  MBEDTLS_CIPHER_ID_AES,
} mbedtls_cipher_id_t;

#define MBEDTLS_GCM_ENCRYPT 1
#define MBEDTLS_GCM_DECRYPT 0

struct mbedtls_gcm_context {
  uint8_t key[32];
  size_t keyLength;
  bool ready;
};

void mbedtls_gcm_init(mbedtls_gcm_context* context);
void mbedtls_gcm_free(mbedtls_gcm_context* context);
int mbedtls_gcm_setkey(mbedtls_gcm_context* context, mbedtls_cipher_id_t cipher,
                       const unsigned char* key, unsigned int keyBits);
int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* context, int mode, size_t length,
                              const unsigned char* iv, size_t ivLength,
                              const unsigned char* add, size_t addLength,
                              const unsigned char* input, unsigned char* output,
                              size_t tagLength, unsigned char* tag);
int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* context, size_t length,
                             const unsigned char* iv, size_t ivLength,
                             const unsigned char* add, size_t addLength,
                             const unsigned char* tag, size_t tagLength,
                             const unsigned char* input, unsigned char* output);

#endif
