// Host shim for the mbedTLS message-digest API. Only HMAC-SHA256 is provided,
// implemented on top of OpenSSL's SHA-256 primitive.
#ifndef GLOW_SHIM_MBEDTLS_MD_H
#define GLOW_SHIM_MBEDTLS_MD_H

#include <cstddef>
#include <cstdint>
#include <string>

typedef enum {
  MBEDTLS_MD_NONE = 0,
  MBEDTLS_MD_SHA256,
} mbedtls_md_type_t;

struct mbedtls_md_info_t {
  mbedtls_md_type_t type;
  size_t size;
};

struct mbedtls_md_context_t {
  const mbedtls_md_info_t* info;
  bool hmac;
  bool started;
  std::string key;
  std::string message;
};

const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t type);
unsigned char mbedtls_md_get_size(const mbedtls_md_info_t* info);

void mbedtls_md_init(mbedtls_md_context_t* context);
void mbedtls_md_free(mbedtls_md_context_t* context);
int mbedtls_md_setup(mbedtls_md_context_t* context, const mbedtls_md_info_t* info, int hmac);

int mbedtls_md_hmac(const mbedtls_md_info_t* info, const unsigned char* key, size_t keyLength,
                    const unsigned char* input, size_t inputLength, unsigned char* output);
int mbedtls_md_hmac_starts(mbedtls_md_context_t* context, const unsigned char* key,
                           size_t keyLength);
int mbedtls_md_hmac_update(mbedtls_md_context_t* context, const unsigned char* input,
                           size_t inputLength);
int mbedtls_md_hmac_finish(mbedtls_md_context_t* context, unsigned char* output);

#endif
