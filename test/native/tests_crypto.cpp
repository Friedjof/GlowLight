// Known-answer tests for the primitives and the wire format.

#include <cstring>

#include "access.h"
#include "support.h"

using namespace glowtest;

namespace {

Bytes firmwareHkdf(const Bytes& ikm, const Bytes& salt, const Bytes& info, size_t length) {
  Bytes output(length);
  bool ok = GlowSecureTestAccess::hkdfSha256(ikm.data(), ikm.size(), salt.data(),
                                            salt.size(), info.data(), info.size(),
                                            output.data(), length);
  CHECK(ok);
  return output;
}

}  // namespace

// RFC 5869, appendix A.1 — basic SHA-256 test case.
GLOW_TEST(hkdf_matches_rfc5869_case_1) {
  Bytes ikm = fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
  Bytes salt = fromHex("000102030405060708090a0b0c");
  Bytes info = fromHex("f0f1f2f3f4f5f6f7f8f9");

  CHECK_EQ(toHex(firmwareHkdf(ikm, salt, info, 42)),
           std::string("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
                       "34007208d5b887185865"));
}

// RFC 5869, appendix A.2 — longer inputs and output spanning several blocks.
GLOW_TEST(hkdf_matches_rfc5869_case_2) {
  Bytes ikm = fromHex(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f");
  Bytes salt = fromHex(
      "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf");
  Bytes info = fromHex(
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

  CHECK_EQ(toHex(firmwareHkdf(ikm, salt, info, 82)),
           std::string("b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
                       "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
                       "cc30c58179ec3e87c14c01d5c1f3434f1d87"));
}

// RFC 5869, appendix A.3 — empty salt and info.
GLOW_TEST(hkdf_matches_rfc5869_case_3) {
  Bytes ikm = fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");

  CHECK_EQ(toHex(firmwareHkdf(ikm, {}, {}, 42)),
           std::string("8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
                       "9d201395faa4b61a96c8"));
}

// The firmware's HKDF and OpenSSL's must agree on the actual derivations used.
GLOW_TEST(hkdf_matches_openssl_for_protocol_derivations) {
  Bytes groupKey = testGroupKey();
  Bytes salt(reinterpret_cast<const uint8_t*>("GlowLight ESP-NOW v1"),
             reinterpret_cast<const uint8_t*>("GlowLight ESP-NOW v1") + 20);

  Bytes groupTagInfo(reinterpret_cast<const uint8_t*>("group-tag"),
                     reinterpret_cast<const uint8_t*>("group-tag") + 9);
  CHECK_EQ(toHex(firmwareHkdf(groupKey, salt, groupTagInfo, kGroupTagSize)),
           toHex(groupTagFor(groupKey)));

  Bytes mac = fromHex("aabbccddeeff");
  Bytes bootId = fromHex("000102030405060708090a0b0c0d0e0f");
  Bytes bootKeyInfo(reinterpret_cast<const uint8_t*>("boot-key"),
                    reinterpret_cast<const uint8_t*>("boot-key") + 8);
  bootKeyInfo.insert(bootKeyInfo.end(), mac.begin(), mac.end());
  bootKeyInfo.insert(bootKeyInfo.end(), bootId.begin(), bootId.end());
  CHECK_EQ(toHex(firmwareHkdf(groupKey, salt, bootKeyInfo, kKeySize)),
           toHex(bootKeyFor(groupKey, mac.data(), bootId.data())));
}

// NIST CAVP gcmEncryptExtIV256, keylen 256 / ivlen 96 / ptlen 0 / aadlen 0,
// count 0. Pins the AES-GCM used by the test peer, which is the oracle every
// other test compares the firmware against.
GLOW_TEST(aes_gcm_matches_nist_vector) {
  Bytes key = fromHex("b52c505a37d78eda5dd34f20c22540ea1b58963cf8e5bf8ffa85f9f2492505b4");
  Bytes nonce = fromHex("516c33929df5a3284ff463d7");

  Bytes tag;
  Bytes ciphertext = aesGcmEncrypt(key, nonce, {}, {}, &tag);
  CHECK_EQ(ciphertext.size(), static_cast<size_t>(0));
  CHECK_EQ(toHex(tag), std::string("bdc1ac884d332457a1d2664f168c76f0"));

  // ...and the matching decrypt must accept it and reject a flipped tag.
  Bytes plaintext;
  CHECK(aesGcmDecrypt(key, nonce, {}, {}, tag, &plaintext));
  tag[0] ^= 0x01;
  CHECK(!aesGcmDecrypt(key, nonce, {}, {}, tag, &plaintext));
}

// The exact bytes a lamp puts on the air. Pins magic, version, field offsets,
// group tag, nonce construction and the AAD in a single vector.
GLOW_TEST(golden_hello_frame_is_byte_stable) {
  Device device("983dae52877c", 12345);
  device.pump(1);

  CHECK(device.rawOutboxSize() >= 1);
  const Bytes& frame = glow_shim::sentFrames[0].data;

  CHECK_EQ(frame.size(), kHeaderSize + 0 + kGcmTagSize);
  CHECK_EQ(frame[kOffsetMagic], static_cast<uint8_t>('G'));
  CHECK_EQ(frame[kOffsetMagic + 1], static_cast<uint8_t>('L'));
  CHECK_EQ(frame[kOffsetVersion], static_cast<uint8_t>(1));
  CHECK_EQ(frame[kOffsetType], static_cast<uint8_t>(Frame::HELLO));
  CHECK_EQ(frame[kOffsetReserved1], static_cast<uint8_t>(0));
  CHECK_EQ(frame[kOffsetFragmentIndex], static_cast<uint8_t>(0));
  CHECK_EQ(frame[kOffsetFragmentCount], static_cast<uint8_t>(1));
  CHECK_EQ(frame[kOffsetReserved2], static_cast<uint8_t>(0));

  // Group tag must equal HKDF(groupKey, salt, "group-tag") computed by OpenSSL.
  Bytes expectedTag = groupTagFor(testGroupKey());
  CHECK_EQ(toHex(Bytes(frame.begin() + kOffsetGroupTag,
                       frame.begin() + kOffsetGroupTag + kGroupTagSize)),
           toHex(expectedTag));

  CHECK_EQ(toHex(Bytes(frame.begin() + kOffsetMac, frame.begin() + kOffsetMac + kMacSize)),
           std::string("983dae52877c"));

  // First frame after boot always carries counter 1 and an empty payload.
  CHECK_EQ(toHex(Bytes(frame.begin() + kOffsetCounter, frame.begin() + kOffsetCounter + 8)),
           std::string("0000000000000001"));
  CHECK_EQ(frame[kOffsetLength], static_cast<uint8_t>(0));
  CHECK_EQ(frame[kOffsetLength + 1], static_cast<uint8_t>(0));

  // Independently recompute the tag from the header, boot id and derived key.
  Bytes bootId(frame.begin() + kOffsetBootId, frame.begin() + kOffsetBootId + kBootIdSize);
  Bytes key = bootKeyFor(testGroupKey(), frame.data() + kOffsetMac, bootId.data());
  Bytes aad(frame.begin(), frame.begin() + kHeaderSize);
  Bytes gcmTag;
  aesGcmEncrypt(key, nonceFor(1), aad, {}, &gcmTag);
  CHECK_EQ(toHex(Bytes(frame.begin() + kHeaderSize, frame.end())), toHex(gcmTag));
}

// The header layout must not drift: 48 bytes, 186-byte fragments, 250-byte MTU.
GLOW_TEST(frame_geometry_is_unchanged) {
  CHECK_EQ(kHeaderSize, static_cast<size_t>(48));
  CHECK_EQ(kMaxFragmentSize, static_cast<size_t>(186));
  CHECK_EQ(kHeaderSize + kMaxFragmentSize + kGcmTagSize, kEspNowMtu);
  CHECK(kMaxFragmentSize * 3 >= kMaxPlaintextSize);
}

// ------------------------------------------------------------ group key -----

GLOW_TEST(group_key_accepts_only_64_hex_characters) {
  Device device;
  CommunicationService& service = device.service();

  CHECK(GlowSecureTestAccess::parseGroupKey(service, std::string(64, 'a').c_str()));
  CHECK(GlowSecureTestAccess::parseGroupKey(service, std::string(64, 'F').c_str()));

  CHECK(!GlowSecureTestAccess::parseGroupKey(service, nullptr));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, ""));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, "PROVISION_WITH_SETUP"));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, std::string(63, 'a').c_str()));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, std::string(65, 'a').c_str()));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, std::string(64, 'g').c_str()));
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, (std::string(63, 'a') + " ").c_str()));
}

// The setup validator refuses an all-zero key; the firmware must agree.
GLOW_TEST(group_key_rejects_all_zero) {
  Device device;
  CHECK(!GlowSecureTestAccess::parseGroupKey(device.service(), std::string(64, '0').c_str()));
}

// A rejected key must not leave partial material behind.
GLOW_TEST(group_key_is_cleared_when_parsing_fails) {
  Device device;
  CommunicationService& service = device.service();
  std::string malformed = std::string(62, 'a') + "zz";
  CHECK(!GlowSecureTestAccess::parseGroupKey(service, malformed.c_str()));

  uint8_t key[32];
  GlowSecureTestAccess::groupKey(service, key);
  bool allZero = true;
  for (uint8_t byte : key)
    if (byte != 0) allZero = false;
  CHECK(allZero);
}
