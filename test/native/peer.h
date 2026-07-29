// An independent implementation of the GlowLight secure frame format.
//
// This deliberately does NOT reuse any firmware code: HKDF comes from OpenSSL's
// own KDF, GCM from OpenSSL's EVP, and the header is assembled byte by byte from
// the specification. If the firmware and this file ever disagree, the tests fail.
#ifndef GLOW_TEST_PEER_H
#define GLOW_TEST_PEER_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glowtest {

enum class Frame : uint8_t {
  HELLO = 1,
  CHALLENGE = 2,
  PROOF = 3,
  HEARTBEAT = 4,
  DATA = 5,
};

constexpr size_t kMacSize = 6;
constexpr size_t kBootIdSize = 16;
constexpr size_t kChallengeSize = 16;
constexpr size_t kKeySize = 32;
constexpr size_t kGroupTagSize = 8;
constexpr size_t kGcmTagSize = 16;
constexpr size_t kNonceSize = 12;
constexpr size_t kHeaderSize = 48;
constexpr size_t kEspNowMtu = 250;
constexpr size_t kMaxFragmentSize = kEspNowMtu - kHeaderSize - kGcmTagSize;  // 186
constexpr size_t kMaxPlaintextSize = 512;

// Header field offsets, straight from the specification.
constexpr size_t kOffsetMagic = 0;
constexpr size_t kOffsetVersion = 2;
constexpr size_t kOffsetType = 3;
constexpr size_t kOffsetReserved1 = 4;
constexpr size_t kOffsetFragmentIndex = 5;
constexpr size_t kOffsetFragmentCount = 6;
constexpr size_t kOffsetReserved2 = 7;
constexpr size_t kOffsetGroupTag = 8;
constexpr size_t kOffsetMac = 16;
constexpr size_t kOffsetBootId = 22;
constexpr size_t kOffsetCounter = 38;
constexpr size_t kOffsetLength = 46;

using Bytes = std::vector<uint8_t>;

// HKDF-SHA256 via OpenSSL, used as the reference for the firmware's own version.
Bytes hkdfSha256(const Bytes& ikm, const Bytes& salt, const Bytes& info, size_t length);

Bytes aesGcmEncrypt(const Bytes& key, const Bytes& nonce, const Bytes& aad,
                    const Bytes& plaintext, Bytes* tag);
bool aesGcmDecrypt(const Bytes& key, const Bytes& nonce, const Bytes& aad,
                   const Bytes& ciphertext, const Bytes& tag, Bytes* plaintext);

Bytes groupTagFor(const Bytes& groupKey);
Bytes bootKeyFor(const Bytes& groupKey, const uint8_t* mac, const uint8_t* bootId);
Bytes nonceFor(uint64_t counter);

Bytes fromHex(const std::string& hex);
std::string toHex(const Bytes& bytes);

struct ParsedFrame {
  uint8_t version = 0;
  uint8_t type = 0;
  uint8_t reserved1 = 0;
  uint8_t reserved2 = 0;
  uint8_t fragmentIndex = 0;
  uint8_t fragmentCount = 0;
  std::array<uint8_t, kGroupTagSize> groupTag{};
  std::array<uint8_t, kMacSize> mac{};
  std::array<uint8_t, kBootIdSize> bootId{};
  uint64_t counter = 0;
  Bytes plaintext;

  Frame frameType() const { return static_cast<Frame>(this->type); }
};

// A simulated group member: builds frames as a real lamp would and opens frames
// the device under test transmits.
class Peer {
 public:
  Peer(const std::string& macHex, const std::string& bootIdHex, const Bytes& groupKey);

  Bytes build(Frame type, const Bytes& payload, uint8_t fragmentIndex = 0,
              uint8_t fragmentCount = 1);
  // Same, but with an explicit counter (for replay and out-of-order tests).
  Bytes buildWithCounter(uint64_t counter, Frame type, const Bytes& payload,
                         uint8_t fragmentIndex = 0, uint8_t fragmentCount = 1);

  // Splits a payload across fragments the way the firmware does.
  std::vector<Bytes> buildData(const std::string& payload);

  // Decrypts a frame produced by any group member (the group key is shared).
  bool open(const Bytes& frame, ParsedFrame* parsed) const;

  const std::array<uint8_t, kMacSize>& mac() const { return this->mac_; }
  const std::array<uint8_t, kBootIdSize>& bootId() const { return this->bootId_; }
  uint64_t counter() const { return this->counter_; }
  void setCounter(uint64_t counter) { this->counter_ = counter; }
  void setBootId(const std::string& bootIdHex);

  // Node id the firmware derives from a MAC (folded into 32 bits).
  static uint32_t nodeIdFor(const uint8_t* mac);
  uint32_t nodeId() const { return nodeIdFor(this->mac_.data()); }

 private:
  std::array<uint8_t, kMacSize> mac_{};
  std::array<uint8_t, kBootIdSize> bootId_{};
  Bytes groupKey_;
  uint64_t counter_ = 0;
};

}  // namespace glowtest

#endif
