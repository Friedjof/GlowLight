#include "peer.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace glowtest {
namespace {

const char kHkdfSalt[] = "GlowLight ESP-NOW v1";
const char kGroupTagInfo[] = "group-tag";
const char kBootKeyInfo[] = "boot-key";
const uint8_t kNonceDomain[4] = {'G', 'L', 'W', 1};

Bytes toBytes(const char* text) {
  return Bytes(reinterpret_cast<const uint8_t*>(text),
               reinterpret_cast<const uint8_t*>(text) + strlen(text));
}

void writeUint64(uint8_t* output, uint64_t value) {
  for (int i = 7; i >= 0; --i) {
    output[i] = static_cast<uint8_t>(value);
    value >>= 8;
  }
}

uint64_t readUint64(const uint8_t* input) {
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) value = (value << 8) | input[i];
  return value;
}

}  // namespace

Bytes hkdfSha256(const Bytes& ikm, const Bytes& salt, const Bytes& info, size_t length) {
  EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
  if (kdf == nullptr) throw std::runtime_error("HKDF unavailable");
  EVP_KDF_CTX* context = EVP_KDF_CTX_new(kdf);
  EVP_KDF_free(kdf);
  if (context == nullptr) throw std::runtime_error("HKDF context failed");

  char digest[] = "SHA256";
  OSSL_PARAM params[] = {
      OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                        const_cast<uint8_t*>(ikm.data()), ikm.size()),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                        const_cast<uint8_t*>(salt.data()), salt.size()),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                        const_cast<uint8_t*>(info.data()), info.size()),
      OSSL_PARAM_construct_end()};

  Bytes output(length);
  int result = EVP_KDF_derive(context, output.data(), length, params);
  EVP_KDF_CTX_free(context);
  if (result != 1) throw std::runtime_error("HKDF derive failed");
  return output;
}

Bytes aesGcmEncrypt(const Bytes& key, const Bytes& nonce, const Bytes& aad,
                    const Bytes& plaintext, Bytes* tag) {
  EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
  if (cipher == nullptr) throw std::runtime_error("cipher context failed");

  Bytes ciphertext(plaintext.size());
  int length = 0;
  bool ok = EVP_EncryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(nonce.size()), nullptr) == 1 &&
            EVP_EncryptInit_ex(cipher, nullptr, nullptr, key.data(), nonce.data()) == 1;
  if (ok && !aad.empty())
    ok = EVP_EncryptUpdate(cipher, nullptr, &length, aad.data(),
                           static_cast<int>(aad.size())) == 1;
  if (ok && !plaintext.empty())
    ok = EVP_EncryptUpdate(cipher, ciphertext.data(), &length, plaintext.data(),
                           static_cast<int>(plaintext.size())) == 1;
  if (ok) {
    int finalLength = 0;
    tag->assign(kGcmTagSize, 0);
    ok = EVP_EncryptFinal_ex(cipher, ciphertext.data() + length, &finalLength) == 1 &&
         EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kGcmTagSize),
                             tag->data()) == 1;
  }
  EVP_CIPHER_CTX_free(cipher);
  if (!ok) throw std::runtime_error("GCM encrypt failed");
  return ciphertext;
}

bool aesGcmDecrypt(const Bytes& key, const Bytes& nonce, const Bytes& aad,
                   const Bytes& ciphertext, const Bytes& tag, Bytes* plaintext) {
  EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
  if (cipher == nullptr) return false;

  plaintext->assign(ciphertext.size(), 0);
  int length = 0;
  bool ok = EVP_DecryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(nonce.size()), nullptr) == 1 &&
            EVP_DecryptInit_ex(cipher, nullptr, nullptr, key.data(), nonce.data()) == 1;
  if (ok && !aad.empty())
    ok = EVP_DecryptUpdate(cipher, nullptr, &length, aad.data(),
                           static_cast<int>(aad.size())) == 1;
  if (ok && !ciphertext.empty())
    ok = EVP_DecryptUpdate(cipher, plaintext->data(), &length, ciphertext.data(),
                           static_cast<int>(ciphertext.size())) == 1;
  if (ok)
    ok = EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
                             const_cast<uint8_t*>(tag.data())) == 1;
  if (ok) {
    int finalLength = 0;
    ok = EVP_DecryptFinal_ex(cipher, plaintext->data() + length, &finalLength) == 1;
  }
  EVP_CIPHER_CTX_free(cipher);
  return ok;
}

Bytes groupTagFor(const Bytes& groupKey) {
  return hkdfSha256(groupKey, toBytes(kHkdfSalt), toBytes(kGroupTagInfo), kGroupTagSize);
}

Bytes bootKeyFor(const Bytes& groupKey, const uint8_t* mac, const uint8_t* bootId) {
  Bytes info = toBytes(kBootKeyInfo);
  info.insert(info.end(), mac, mac + kMacSize);
  info.insert(info.end(), bootId, bootId + kBootIdSize);
  return hkdfSha256(groupKey, toBytes(kHkdfSalt), info, kKeySize);
}

Bytes nonceFor(uint64_t counter) {
  Bytes nonce(kNonceSize);
  memcpy(nonce.data(), kNonceDomain, sizeof(kNonceDomain));
  writeUint64(nonce.data() + sizeof(kNonceDomain), counter);
  return nonce;
}

Bytes fromHex(const std::string& hex) {
  if (hex.size() % 2 != 0) throw std::runtime_error("odd hex string");
  Bytes bytes(hex.size() / 2);
  for (size_t i = 0; i < bytes.size(); ++i) {
    unsigned value = 0;
    if (sscanf(hex.c_str() + i * 2, "%2x", &value) != 1)
      throw std::runtime_error("invalid hex string");
    bytes[i] = static_cast<uint8_t>(value);
  }
  return bytes;
}

std::string toHex(const Bytes& bytes) {
  static const char* digits = "0123456789abcdef";
  std::string hex;
  hex.reserve(bytes.size() * 2);
  for (uint8_t byte : bytes) {
    hex.push_back(digits[byte >> 4]);
    hex.push_back(digits[byte & 0x0f]);
  }
  return hex;
}

uint32_t Peer::nodeIdFor(const uint8_t* mac) {
  uint32_t id = 0;
  id |= static_cast<uint32_t>(mac[3]) << 24;
  id |= static_cast<uint32_t>(mac[4]) << 16;
  id |= static_cast<uint32_t>(mac[5]) << 8;
  id |= static_cast<uint8_t>(mac[0] ^ mac[1] ^ mac[2]);
  return id;
}

Peer::Peer(const std::string& macHex, const std::string& bootIdHex, const Bytes& groupKey)
    : groupKey_(groupKey) {
  Bytes mac = fromHex(macHex);
  Bytes bootId = fromHex(bootIdHex);
  if (mac.size() != kMacSize || bootId.size() != kBootIdSize)
    throw std::runtime_error("bad peer identity");
  memcpy(this->mac_.data(), mac.data(), kMacSize);
  memcpy(this->bootId_.data(), bootId.data(), kBootIdSize);
}

void Peer::setBootId(const std::string& bootIdHex) {
  Bytes bootId = fromHex(bootIdHex);
  if (bootId.size() != kBootIdSize) throw std::runtime_error("bad boot id");
  memcpy(this->bootId_.data(), bootId.data(), kBootIdSize);
  this->counter_ = 0;
}

Bytes Peer::buildWithCounter(uint64_t counter, Frame type, const Bytes& payload,
                             uint8_t fragmentIndex, uint8_t fragmentCount) {
  Bytes frame(kHeaderSize);
  frame[kOffsetMagic] = 'G';
  frame[kOffsetMagic + 1] = 'L';
  frame[kOffsetVersion] = 1;
  frame[kOffsetType] = static_cast<uint8_t>(type);
  frame[kOffsetReserved1] = 0;
  frame[kOffsetFragmentIndex] = fragmentIndex;
  frame[kOffsetFragmentCount] = fragmentCount;
  frame[kOffsetReserved2] = 0;

  Bytes tag = groupTagFor(this->groupKey_);
  memcpy(frame.data() + kOffsetGroupTag, tag.data(), kGroupTagSize);
  memcpy(frame.data() + kOffsetMac, this->mac_.data(), kMacSize);
  memcpy(frame.data() + kOffsetBootId, this->bootId_.data(), kBootIdSize);
  writeUint64(frame.data() + kOffsetCounter, counter);
  frame[kOffsetLength] = static_cast<uint8_t>(payload.size() >> 8);
  frame[kOffsetLength + 1] = static_cast<uint8_t>(payload.size());

  Bytes key = bootKeyFor(this->groupKey_, this->mac_.data(), this->bootId_.data());
  Bytes gcmTag;
  Bytes ciphertext = aesGcmEncrypt(key, nonceFor(counter), frame, payload, &gcmTag);

  frame.insert(frame.end(), ciphertext.begin(), ciphertext.end());
  frame.insert(frame.end(), gcmTag.begin(), gcmTag.end());
  return frame;
}

Bytes Peer::build(Frame type, const Bytes& payload, uint8_t fragmentIndex,
                  uint8_t fragmentCount) {
  return this->buildWithCounter(++this->counter_, type, payload, fragmentIndex,
                                fragmentCount);
}

std::vector<Bytes> Peer::buildData(const std::string& payload) {
  size_t fragmentCount = (payload.size() + kMaxFragmentSize - 1) / kMaxFragmentSize;
  if (fragmentCount == 0) fragmentCount = 1;

  std::vector<Bytes> frames;
  for (size_t i = 0; i < fragmentCount; ++i) {
    size_t offset = i * kMaxFragmentSize;
    size_t length = payload.size() - offset < kMaxFragmentSize ? payload.size() - offset
                                                               : kMaxFragmentSize;
    Bytes fragment(reinterpret_cast<const uint8_t*>(payload.data()) + offset,
                   reinterpret_cast<const uint8_t*>(payload.data()) + offset + length);
    frames.push_back(this->build(Frame::DATA, fragment, static_cast<uint8_t>(i),
                                 static_cast<uint8_t>(fragmentCount)));
  }
  return frames;
}

bool Peer::open(const Bytes& frame, ParsedFrame* parsed) const {
  if (frame.size() < kHeaderSize + kGcmTagSize) return false;

  size_t length = (static_cast<size_t>(frame[kOffsetLength]) << 8) | frame[kOffsetLength + 1];
  if (frame.size() != kHeaderSize + length + kGcmTagSize) return false;

  parsed->version = frame[kOffsetVersion];
  parsed->type = frame[kOffsetType];
  parsed->reserved1 = frame[kOffsetReserved1];
  parsed->reserved2 = frame[kOffsetReserved2];
  parsed->fragmentIndex = frame[kOffsetFragmentIndex];
  parsed->fragmentCount = frame[kOffsetFragmentCount];
  memcpy(parsed->groupTag.data(), frame.data() + kOffsetGroupTag, kGroupTagSize);
  memcpy(parsed->mac.data(), frame.data() + kOffsetMac, kMacSize);
  memcpy(parsed->bootId.data(), frame.data() + kOffsetBootId, kBootIdSize);
  parsed->counter = readUint64(frame.data() + kOffsetCounter);

  Bytes key = bootKeyFor(this->groupKey_, parsed->mac.data(), parsed->bootId.data());
  Bytes aad(frame.begin(), frame.begin() + kHeaderSize);
  Bytes ciphertext(frame.begin() + kHeaderSize, frame.begin() + kHeaderSize + length);
  Bytes tag(frame.begin() + kHeaderSize + length, frame.end());
  return aesGcmDecrypt(key, nonceFor(parsed->counter), aad, ciphertext, tag,
                       &parsed->plaintext);
}

}  // namespace glowtest
