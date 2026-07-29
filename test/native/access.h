// Access to the few private helpers the tests need to exercise directly.
// CommunicationService declares this struct as a friend under GLOW_UNIT_TEST.
#ifndef GLOW_TEST_ACCESS_H
#define GLOW_TEST_ACCESS_H

#include "CommunicationService.h"

struct GlowSecureTestAccess {
  static bool hkdfSha256(const uint8_t* ikm, size_t ikmLength, const uint8_t* salt,
                         size_t saltLength, const uint8_t* info, size_t infoLength,
                         uint8_t* output, size_t outputLength) {
    return CommunicationService::hkdfSha256(ikm, ikmLength, salt, saltLength, info,
                                            infoLength, output, outputLength);
  }

  static bool parseGroupKey(CommunicationService& service, const char* hex) {
    return service.parseGroupKey(hex);
  }

  static void groupKey(CommunicationService& service, uint8_t* output) {
    memcpy(output, service.groupKey, CommunicationService::KEY_SIZE);
  }

  // Number of session slots currently in use, and how many are authenticated.
  static size_t usedSessions(const CommunicationService& service) {
    size_t count = 0;
    for (const auto& session : service.sessions)
      if (session.used) ++count;
    return count;
  }

  static size_t establishedSessions(const CommunicationService& service) {
    size_t count = 0;
    for (const auto& session : service.sessions)
      if (session.used && session.established) ++count;
    return count;
  }

  static uint64_t replayMax(const CommunicationService& service, const uint8_t* mac) {
    for (const auto& session : service.sessions)
      if (session.used && memcmp(session.mac, mac, CommunicationService::MAC_SIZE) == 0)
        return session.replayMax;
    return 0;
  }

  static uint64_t replayBitmap(const CommunicationService& service, const uint8_t* mac) {
    for (const auto& session : service.sessions)
      if (session.used && memcmp(session.mac, mac, CommunicationService::MAC_SIZE) == 0)
        return session.replayBitmap;
    return 0;
  }
};

#endif
