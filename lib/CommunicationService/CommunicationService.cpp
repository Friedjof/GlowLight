#include "CommunicationService.h"

#include <esp_system.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>

#ifndef GLOW_GROUP_KEY_HEX
#define GLOW_GROUP_KEY_HEX ""
#endif

#ifndef GLOW_HEARTBEAT_INTERVAL
#define GLOW_HEARTBEAT_INTERVAL HARTBEAT_INTERVAL
#endif

namespace {
constexpr uint8_t FRAME_MAGIC[2] = {'G', 'L'};
constexpr uint8_t FRAME_VERSION = 1;
constexpr uint8_t NONCE_DOMAIN[4] = {'G', 'L', 'W', 1};
constexpr uint8_t HKDF_SALT[] = "GlowLight ESP-NOW v1";
constexpr uint8_t GROUP_TAG_INFO[] = "group-tag";
constexpr uint8_t BOOT_KEY_INFO[] = "boot-key";
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
}

CommunicationService* CommunicationService::instance = nullptr;

CommunicationService::CommunicationService() = default;

CommunicationService::~CommunicationService() {
  if (instance == this) instance = nullptr;
  if (this->espNowInitialized) esp_now_deinit();
  if (this->rawReceiveQueue != nullptr) vQueueDelete(this->rawReceiveQueue);
  if (this->receiveQueue != nullptr) vQueueDelete(this->receiveQueue);
  if (this->sendQueue != nullptr) vQueueDelete(this->sendQueue);
  memset(this->groupKey, 0, sizeof(this->groupKey));
  memset(this->localKey, 0, sizeof(this->localKey));
}

void CommunicationService::setup() { this->setup(CommunicationConfig()); }

void CommunicationService::setup(const CommunicationConfig& config) {
  this->runtimeEnabled = config.enabled;

  // The node identity is a property of the hardware, not of the transport.
  // Other services use it to name this lamp, so it has to exist even when the
  // group communication is switched off. NetworkService owns the radio: it puts
  // the interface into station mode and decides the channel, so nothing here
  // may touch the mode or disconnect.
  WiFi.macAddress(this->localMac);
  this->localNodeId = this->macToNodeId(this->localMac);

  if (!this->runtimeEnabled) {
    Serial.println("[INFO] Communication disabled");
    return;
  }

  if (!this->parseGroupKey(config.groupKeyHex.c_str()) || !this->deriveGroupTag()) {
    Serial.println("[ERROR] Communication disabled: invalid group key");
    return;
  }
  this->keyValid = true;

  esp_fill_random(this->bootId, sizeof(this->bootId));

  // Our own boot key never changes while we run, so derive it once.
  if (!this->deriveBootKey(this->localMac, this->bootId, this->localKey)) {
    Serial.println("[ERROR] Communication disabled: key derivation failed");
    return;
  }

  this->rawReceiveQueue = xQueueCreate(RECEIVE_QUEUE_LENGTH, sizeof(RawFrame));
  this->receiveQueue = xQueueCreate(RECEIVE_QUEUE_LENGTH, sizeof(PendingMessage));
  this->sendQueue = xQueueCreate(SEND_QUEUE_LENGTH, sizeof(PendingTransmission));
  if (this->rawReceiveQueue == nullptr || this->receiveQueue == nullptr ||
      this->sendQueue == nullptr) {
    Serial.println("[ERROR] Communication disabled: queue allocation failed");
    return;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] Communication disabled: ESP-NOW init failed");
    return;
  }
  this->espNowInitialized = true;
  instance = this;

  if (esp_now_register_recv_cb(CommunicationService::onDataRecv) != ESP_OK ||
      esp_now_register_send_cb(CommunicationService::onDataSent) != ESP_OK) {
    Serial.println("[ERROR] Communication disabled: callback registration failed");
    instance = nullptr;
    this->espNowInitialized = false;
    esp_now_deinit();
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  // Zero means "whatever channel the radio is on". With WiFi active the access
  // point dictates the channel, so pinning it here would break transmission.
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ERROR] Communication disabled: broadcast peer failed");
    instance = nullptr;
    this->espNowInitialized = false;
    esp_now_deinit();
    return;
  }

  this->sendHello();
  Serial.printf("[INFO] Secure communication initialized, node %u\n", this->localNodeId);
}

void CommunicationService::loop() {
  if (!this->runtimeEnabled || !this->espNowInitialized || !this->keyValid) return;

  RawFrame raw;
  while (xQueueReceive(this->rawReceiveQueue, &raw, 0) == pdTRUE) {
    this->processRawFrame(raw);
  }

  PendingMessage pending;
  while (xQueueReceive(this->receiveQueue, &pending, 0) == pdTRUE) {
    String message(pending.payload);
    this->receivedCallback(pending.senderNodeId, message);
  }

  this->processPendingChallenges();
  this->processSendQueue();

  if (millis() - this->lastHeartbeat >= GLOW_HEARTBEAT_INTERVAL) {
    this->lastHeartbeat = millis();
    this->sendHello();
    this->queueFrame(FrameType::HEARTBEAT, nullptr, 0);
  }
  this->removeOldNodes();
}

bool CommunicationService::isAvailable() const {
  return this->runtimeEnabled && this->espNowInitialized && this->keyValid;
}

bool CommunicationService::setApplicationPublishing(bool enabled) {
  this->applicationPublishing = enabled;
  if (enabled || this->sendQueue == nullptr) return false;

  bool droppedApplication = false;
  UBaseType_t queued = uxQueueMessagesWaiting(this->sendQueue);
  for (UBaseType_t i = 0; i < queued; ++i) {
    PendingTransmission transmission;
    if (xQueueReceive(this->sendQueue, &transmission, 0) != pdTRUE) break;
    if (transmission.application) {
      droppedApplication = true;
    } else {
      xQueueSend(this->sendQueue, &transmission, 0);
    }
  }
  return droppedApplication;
}

uint64_t CommunicationService::nextRevision() const {
  return this->syncClock == UINT64_MAX ? UINT64_MAX : this->syncClock + 1;
}

void CommunicationService::acceptSyncVersion(const SyncVersion& version) {
  if (version.revision > this->syncClock) {
    if (version.revision - this->syncClock > MAX_SYNC_ADVANCE) {
      // Adopting this would strand the clock near the wrap-around point.
      Serial.printf("[WARN] Implausible sync revision from node %u, clock not advanced\n",
                    version.origin);
      this->currentSyncVersion = version;
      return;
    }
    this->syncClock = version.revision;
  }
  this->currentSyncVersion = version;
}

void CommunicationService::radioChannelChanged(uint8_t channel) {
  if (!this->runtimeEnabled || !this->espNowInitialized || !this->keyValid) return;
  Serial.printf("[INFO] ESP-NOW moved to channel %u, announcing node\n", channel);
  this->sendHello();
}

bool CommunicationService::parseGroupKey(const char* hex) {
  if (hex == nullptr || strlen(hex) != KEY_SIZE * 2) return false;

  bool allZero = true;
  for (size_t i = 0; i < KEY_SIZE; ++i) {
    char high = hex[i * 2];
    char low = hex[i * 2 + 1];
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int h = nibble(high);
    int l = nibble(low);
    if (h < 0 || l < 0) {
      memset(this->groupKey, 0, sizeof(this->groupKey));
      return false;
    }
    this->groupKey[i] = static_cast<uint8_t>((h << 4) | l);
    if (this->groupKey[i] != 0) allZero = false;
  }

  // An all-zero key is the classic placeholder; the setup validator rejects it
  // as well, so refuse it here instead of joining a trivially guessable group.
  if (allZero) {
    memset(this->groupKey, 0, sizeof(this->groupKey));
    return false;
  }
  return true;
}

bool CommunicationService::deriveGroupTag() {
  return hkdfSha256(this->groupKey, sizeof(this->groupKey), HKDF_SALT,
                    sizeof(HKDF_SALT) - 1, GROUP_TAG_INFO, sizeof(GROUP_TAG_INFO) - 1,
                    this->groupTag, sizeof(this->groupTag));
}

bool CommunicationService::deriveBootKey(const uint8_t* mac, const uint8_t* remoteBootId,
                                         uint8_t* key) const {
  uint8_t info[sizeof(BOOT_KEY_INFO) - 1 + MAC_SIZE + BOOT_ID_SIZE];
  size_t prefixLength = sizeof(BOOT_KEY_INFO) - 1;
  memcpy(info, BOOT_KEY_INFO, prefixLength);
  memcpy(info + prefixLength, mac, MAC_SIZE);
  memcpy(info + prefixLength + MAC_SIZE, remoteBootId, BOOT_ID_SIZE);
  return hkdfSha256(this->groupKey, sizeof(this->groupKey), HKDF_SALT,
                    sizeof(HKDF_SALT) - 1, info, sizeof(info), key, KEY_SIZE);
}

bool CommunicationService::hkdfSha256(const uint8_t* ikm, size_t ikmLength,
                                      const uint8_t* salt, size_t saltLength,
                                      const uint8_t* info, size_t infoLength,
                                      uint8_t* output, size_t outputLength) {
  if (outputLength == 0 || outputLength > 255 * 32) return false;
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) return false;

  uint8_t prk[32];
  if (mbedtls_md_hmac(md, salt, saltLength, ikm, ikmLength, prk) != 0) return false;

  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  if (mbedtls_md_setup(&context, md, 1) != 0) {
    memset(prk, 0, sizeof(prk));
    mbedtls_md_free(&context);
    return false;
  }

  uint8_t block[32] = {};
  size_t generated = 0;
  size_t previousLength = 0;
  uint8_t round = 1;
  bool ok = true;
  while (generated < outputLength) {
    if (mbedtls_md_hmac_starts(&context, prk, sizeof(prk)) != 0 ||
        (previousLength > 0 &&
         mbedtls_md_hmac_update(&context, block, previousLength) != 0) ||
        (infoLength > 0 && mbedtls_md_hmac_update(&context, info, infoLength) != 0) ||
        mbedtls_md_hmac_update(&context, &round, 1) != 0 ||
        mbedtls_md_hmac_finish(&context, block) != 0) {
      ok = false;
      break;
    }
    previousLength = sizeof(block);
    size_t take = min(outputLength - generated, sizeof(block));
    memcpy(output + generated, block, take);
    generated += take;
    ++round;
  }

  memset(prk, 0, sizeof(prk));
  memset(block, 0, sizeof(block));
  mbedtls_md_free(&context);
  return ok;
}

void CommunicationService::writeUint16(uint8_t* output, uint16_t value) {
  output[0] = static_cast<uint8_t>(value >> 8);
  output[1] = static_cast<uint8_t>(value);
}

void CommunicationService::writeUint64(uint8_t* output, uint64_t value) {
  for (int i = 7; i >= 0; --i) {
    output[i] = static_cast<uint8_t>(value);
    value >>= 8;
  }
}

uint16_t CommunicationService::readUint16(const uint8_t* input) {
  return (static_cast<uint16_t>(input[0]) << 8) | input[1];
}

uint64_t CommunicationService::readUint64(const uint8_t* input) {
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) value = (value << 8) | input[i];
  return value;
}

void CommunicationService::buildNonce(uint64_t counter, uint8_t* nonce) {
  memcpy(nonce, NONCE_DOMAIN, sizeof(NONCE_DOMAIN));
  writeUint64(nonce + sizeof(NONCE_DOMAIN), counter);
}

bool CommunicationService::encryptFrame(FrameType type, uint64_t counter,
                                         const uint8_t* plaintext,
                                         uint16_t plaintextLength, uint8_t fragmentIndex,
                                         uint8_t fragmentCount,
                                         PendingTransmission* transmission) {
  if (plaintextLength > MAX_FRAGMENT_SIZE || fragmentCount == 0 ||
      fragmentCount > MAX_FRAGMENT_COUNT || fragmentIndex >= fragmentCount) return false;

  uint8_t* header = transmission->data;
  header[0] = FRAME_MAGIC[0];
  header[1] = FRAME_MAGIC[1];
  header[2] = FRAME_VERSION;
  header[3] = static_cast<uint8_t>(type);
  header[4] = 0;
  header[5] = fragmentIndex;
  header[6] = fragmentCount;
  header[7] = 0;
  memcpy(header + 8, this->groupTag, GROUP_TAG_SIZE);
  memcpy(header + 16, this->localMac, MAC_SIZE);
  memcpy(header + 22, this->bootId, BOOT_ID_SIZE);
  writeUint64(header + 38, counter);
  writeUint16(header + 46, plaintextLength);

  uint8_t nonce[NONCE_SIZE];
  buildNonce(counter, nonce);
  uint8_t emptyPlaintext = 0;
  if (plaintext == nullptr) plaintext = &emptyPlaintext;

  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  int result =
      mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, this->localKey, KEY_SIZE * 8);
  if (result == 0) {
    result = mbedtls_gcm_crypt_and_tag(
        &context, MBEDTLS_GCM_ENCRYPT, plaintextLength, nonce, sizeof(nonce), header,
        HEADER_SIZE, plaintext, transmission->data + HEADER_SIZE, GCM_TAG_SIZE,
        transmission->data + HEADER_SIZE + plaintextLength);
  }
  mbedtls_gcm_free(&context);
  if (result != 0) return false;

  transmission->length = HEADER_SIZE + plaintextLength + GCM_TAG_SIZE;
  return true;
}

bool CommunicationService::queueFrame(FrameType type, const uint8_t* plaintext,
                                       uint16_t plaintextLength, uint8_t fragmentIndex,
                                       uint8_t fragmentCount, bool application) {
  if (!this->espNowInitialized || this->sendQueue == nullptr ||
      this->sendCounter == UINT64_MAX) return false;

  PendingTransmission transmission;
  transmission.application = application;
  uint64_t counter = ++this->sendCounter;
  if (!this->encryptFrame(type, counter, plaintext, plaintextLength, fragmentIndex,
                          fragmentCount, &transmission)) return false;
  if (xQueueSend(this->sendQueue, &transmission, 0) != pdTRUE) {
    Serial.println("[WARN] Secure TX queue full, frame dropped");
    return false;
  }
  return true;
}

void CommunicationService::processSendQueue() {
  if (this->sendQueue == nullptr) return;

  if (this->sendInProgress.load()) {
    // The driver owes us a completion callback. If it never arrives the node
    // would go silent forever, so give up on the frame after a short timeout.
    if (millis() - this->sendStartedAt < SEND_TIMEOUT_MS) return;
    this->sendInProgress.store(false);
    Serial.println("[WARN] Secure send timed out, transmit path released");
  }

  do {
    if (xQueueReceive(this->sendQueue, &this->activeTransmission, 0) != pdTRUE) return;
  } while (this->activeTransmission.application && !this->applicationPublishing);

#ifdef GLOW_INTEGRATION_TEST
  if (this->traceFrames) {
    Serial.print("[TEST] TX|");
    for (uint16_t i = 0; i < this->activeTransmission.length; ++i)
      Serial.printf("%02x", this->activeTransmission.data[i]);
    Serial.println("");
  }
#endif

  this->sendStartedAt = millis();
  this->sendInProgress.store(true);
  esp_err_t result = esp_now_send(BROADCAST_MAC, this->activeTransmission.data,
                                  this->activeTransmission.length);
  if (result != ESP_OK) {
    this->sendInProgress.store(false);
    Serial.printf("[WARN] Secure send rejected: %d\n", result);
  }
}

void CommunicationService::onDataSent(const uint8_t*, esp_now_send_status_t status) {
  if (instance == nullptr) return;
  instance->sendInProgress.store(false);
  if (status != ESP_NOW_SEND_SUCCESS) Serial.println("[WARN] Secure send not delivered");
}

void CommunicationService::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (instance == nullptr || instance->rawReceiveQueue == nullptr || mac == nullptr ||
      data == nullptr || len < static_cast<int>(HEADER_SIZE + GCM_TAG_SIZE) ||
      len > ESP_NOW_MAX_DATA_LEN) return;

  RawFrame raw;
  memcpy(raw.sourceMac, mac, MAC_SIZE);
  raw.length = static_cast<uint16_t>(len);
  memcpy(raw.data, data, len);
  xQueueSend(instance->rawReceiveQueue, &raw, 0);
}

void CommunicationService::processRawFrame(const RawFrame& raw) {
  const uint8_t* header = raw.data;
  if (header[0] != FRAME_MAGIC[0] || header[1] != FRAME_MAGIC[1] ||
      header[2] != FRAME_VERSION || header[4] != 0 || header[7] != 0 ||
      memcmp(header + 8, this->groupTag, GROUP_TAG_SIZE) != 0 ||
      memcmp(header + 16, raw.sourceMac, MAC_SIZE) != 0 ||
      memcmp(header + 16, this->localMac, MAC_SIZE) == 0) return;

  FrameType type = static_cast<FrameType>(header[3]);
  if (type < FrameType::HELLO || type > FrameType::DATA) return;
  uint8_t fragmentIndex = header[5];
  uint8_t fragmentCount = header[6];
  uint16_t plaintextLength = readUint16(header + 46);
  if (plaintextLength > MAX_FRAGMENT_SIZE ||
      raw.length != HEADER_SIZE + plaintextLength + GCM_TAG_SIZE ||
      fragmentCount == 0 || fragmentCount > MAX_FRAGMENT_COUNT ||
      fragmentIndex >= fragmentCount) return;
  if (type != FrameType::DATA && (fragmentIndex != 0 || fragmentCount != 1)) return;

  const uint8_t* senderMac = header + 16;
  const uint8_t* remoteBootId = header + 22;
  uint64_t counter = readUint64(header + 38);
  if (counter == 0) return;

  // Pick the key without deriving it again for peers we already talk to.
  SenderSession* session = this->findSession(senderMac);
  bool knownBootSession = session != nullptr && session->established &&
                          memcmp(session->bootId, remoteBootId, BOOT_ID_SIZE) == 0;
  uint8_t derivedKey[KEY_SIZE] = {};
  const uint8_t* key = nullptr;
  if (knownBootSession) {
    key = session->key;
  } else if (session != nullptr && session->challengePending &&
             memcmp(session->pendingBootId, remoteBootId, BOOT_ID_SIZE) == 0) {
    key = session->pendingKey;
  } else {
    if (!this->deriveBootKey(senderMac, remoteBootId, derivedKey)) return;
    key = derivedKey;
  }

  uint8_t nonce[NONCE_SIZE];
  buildNonce(counter, nonce);
  uint8_t plaintext[MAX_FRAGMENT_SIZE];

  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, KEY_SIZE * 8);
  if (result == 0) {
    result = mbedtls_gcm_auth_decrypt(
        &context, plaintextLength, nonce, sizeof(nonce), header, HEADER_SIZE,
        raw.data + HEADER_SIZE + plaintextLength, GCM_TAG_SIZE,
        raw.data + HEADER_SIZE, plaintext);
  }
  mbedtls_gcm_free(&context);
  if (result != 0) {
    memset(derivedKey, 0, sizeof(derivedKey));
    return;
  }

  // Everything below runs only on frames whose GCM tag verified.
  if (session == nullptr) {
    session = this->acquireSession(senderMac);
    if (session == nullptr) {
      Serial.println("[WARN] Secure peer capacity reached");
      memset(derivedKey, 0, sizeof(derivedKey));
      return;
    }
  }

  if (knownBootSession) {
    this->handleEstablishedFrame(*session, type, counter, fragmentIndex, fragmentCount,
                                 plaintext, plaintextLength);
  } else {
    this->handleCandidateFrame(*session, remoteBootId, key, type, counter, plaintext,
                               plaintextLength);
  }
  memset(derivedKey, 0, sizeof(derivedKey));
}

CommunicationService::SenderSession* CommunicationService::findSession(const uint8_t* mac) {
  for (auto& session : this->sessions) {
    if (session.used && memcmp(session.mac, mac, MAC_SIZE) == 0) return &session;
  }
  return nullptr;
}

CommunicationService::SenderSession* CommunicationService::acquireSession(
    const uint8_t* mac) {
  uint32_t now = millis();
  SenderSession* freeSlot = nullptr;
  SenderSession* stalest = nullptr;

  for (auto& session : this->sessions) {
    if (session.used && memcmp(session.mac, mac, MAC_SIZE) == 0) return &session;
    if (!session.used) {
      if (freeSlot == nullptr) freeSlot = &session;
      continue;
    }
    // A peer that has been silent past the node timeout releases its slot.
    if (now - session.lastActivityAt > GLOW_NODE_TIMEOUT &&
        (stalest == nullptr || session.lastActivityAt < stalest->lastActivityAt))
      stalest = &session;
  }

  SenderSession* slot = freeSlot != nullptr ? freeSlot : stalest;
  if (slot == nullptr) return nullptr;
  if (slot->used && slot->established) this->removeNode(this->macToNodeId(slot->mac));

  memset(slot, 0, sizeof(*slot));
  slot->used = true;
  slot->lastActivityAt = now;
  memcpy(slot->mac, mac, MAC_SIZE);
  return slot;
}

void CommunicationService::sendHello() {
  this->queueFrame(FrameType::HELLO, nullptr, 0);
}

void CommunicationService::beginChallenge(SenderSession& session,
                                          const uint8_t* remoteBootId, const uint8_t* key,
                                          uint64_t counter) {
  uint32_t now = millis();
  memcpy(session.pendingBootId, remoteBootId, BOOT_ID_SIZE);
  memcpy(session.pendingKey, key, KEY_SIZE);
  session.pendingMaxCounter = counter;
  session.challengePending = true;
  session.lastChallengeRequestAt = now;
  session.challengeCreatedAt = now;
  session.challengeRetries = 0;
  esp_fill_random(session.challenge, sizeof(session.challenge));
  this->sendChallenge(session);
}

void CommunicationService::sendChallenge(SenderSession& session) {
  uint8_t payload[MAC_SIZE + BOOT_ID_SIZE + CHALLENGE_SIZE];
  memcpy(payload, session.mac, MAC_SIZE);
  memcpy(payload + MAC_SIZE, session.pendingBootId, BOOT_ID_SIZE);
  memcpy(payload + MAC_SIZE + BOOT_ID_SIZE, session.challenge, CHALLENGE_SIZE);
  if (this->queueFrame(FrameType::CHALLENGE, payload, sizeof(payload))) {
    session.challengeSentAt = millis();
    ++session.challengeRetries;
  }
}

void CommunicationService::sendProof(const uint8_t* challenge) {
  this->queueFrame(FrameType::PROOF, challenge, CHALLENGE_SIZE);
}

void CommunicationService::processPendingChallenges() {
  uint32_t now = millis();
  for (auto& session : this->sessions) {
    if (!session.used || !session.challengePending) continue;
    if (now - session.challengeCreatedAt >= CHALLENGE_LIFETIME_MS) {
      // Give up on this boot session. The peer's next frame starts a new one.
      session.challengePending = false;
      continue;
    }
    if (session.challengeRetries < MAX_CHALLENGE_RETRIES &&
        now - session.challengeSentAt >= CHALLENGE_RETRY_MS) {
      this->sendChallenge(session);
    }
  }
}

bool CommunicationService::replayAcceptable(const SenderSession& session,
                                             uint64_t counter) const {
  if (counter > session.replayMax) return true;
  uint64_t distance = session.replayMax - counter;
  return distance < 64 && (session.replayBitmap & (UINT64_C(1) << distance)) == 0;
}

void CommunicationService::commitReplay(SenderSession& session, uint64_t counter) {
  if (counter > session.replayMax) {
    uint64_t shift = counter - session.replayMax;
    session.replayBitmap = shift >= 64 ? 1 : (session.replayBitmap << shift) | 1;
    session.replayMax = counter;
  } else {
    session.replayBitmap |= UINT64_C(1) << (session.replayMax - counter);
  }
}

void CommunicationService::promoteSession(SenderSession& session, uint64_t proofCounter) {
  uint32_t nodeId = this->macToNodeId(session.mac);
  bool replacesBootSession = session.established;

  memcpy(session.bootId, session.pendingBootId, BOOT_ID_SIZE);
  memcpy(session.key, session.pendingKey, KEY_SIZE);
  session.established = true;
  session.challengePending = false;
  session.lastActivityAt = millis();
  session.replayMax = proofCounter;
  // The proof counter is the floor: everything at or below it counts as spent,
  // so recorded frames from before the handshake can never be replayed in.
  session.replayBitmap = ~UINT64_C(0);

  // A new boot session restarts the peer's counters; drop stale message state.
  session.reassemblyActive = false;
  session.reassemblyMask = 0;
  session.hasDeliveredMessage = false;
  session.lastDeliveredMessageId = 0;
  memset(session.fragmentLengths, 0, sizeof(session.fragmentLengths));

  if (replacesBootSession) this->removeNode(nodeId);
  bool isNew = !this->updateNode(nodeId);
  if (isNew) {
    Serial.printf("[INFO] Authenticated node %u\n", nodeId);
    if (this->connectionCallback != nullptr) this->connectionCallback(nodeId);
  }
}

void CommunicationService::answerChallenge(const uint8_t* plaintext,
                                           uint16_t plaintextLength) {
  if (plaintextLength != MAC_SIZE + BOOT_ID_SIZE + CHALLENGE_SIZE ||
      memcmp(plaintext, this->localMac, MAC_SIZE) != 0 ||
      memcmp(plaintext + MAC_SIZE, this->bootId, BOOT_ID_SIZE) != 0) return;
  this->sendProof(plaintext + MAC_SIZE + BOOT_ID_SIZE);
}

void CommunicationService::handleEstablishedFrame(SenderSession& session, FrameType type,
                                                  uint64_t counter, uint8_t fragmentIndex,
                                                  uint8_t fragmentCount,
                                                  const uint8_t* plaintext,
                                                  uint16_t plaintextLength) {
  if (!this->replayAcceptable(session, counter)) return;
  this->commitReplay(session, counter);
  session.lastActivityAt = millis();
  this->updateNode(this->macToNodeId(session.mac));

  switch (type) {
    case FrameType::CHALLENGE:
      this->answerChallenge(plaintext, plaintextLength);
      break;
    case FrameType::DATA:
      this->handleData(session, counter, fragmentIndex, fragmentCount, plaintext,
                       plaintextLength);
      break;
    // HELLO and HEARTBEAT only prove liveness; PROOF is redundant once
    // authenticated and must never re-open the handshake.
    default:
      break;
  }
}

void CommunicationService::handleCandidateFrame(SenderSession& session,
                                                const uint8_t* remoteBootId,
                                                const uint8_t* key, FrameType type,
                                                uint64_t counter,
                                                const uint8_t* plaintext,
                                                uint16_t plaintextLength) {
  uint32_t now = millis();
  bool isPending = session.challengePending &&
                   memcmp(session.pendingBootId, remoteBootId, BOOT_ID_SIZE) == 0;

  if (isPending) {
    // A live peer only ever counts upwards; anything else is a replay.
    if (counter <= session.pendingMaxCounter) return;
    session.pendingMaxCounter = counter;
  } else {
    // An unknown boot session. Displacing a challenge that is still in flight
    // is rate limited, so replayed recordings of many old boot sessions cannot
    // make us flood the channel. A peer that reboots while nothing is pending
    // is challenged straight away.
    if (session.challengePending &&
        now - session.lastChallengeRequestAt < UNKNOWN_CHALLENGE_INTERVAL_MS) return;
    this->beginChallenge(session, remoteBootId, key, counter);
  }
  session.lastActivityAt = now;

  if (type == FrameType::CHALLENGE) {
    this->answerChallenge(plaintext, plaintextLength);
    return;
  }

  if (type == FrameType::PROOF) {
    if (!session.challengePending || plaintextLength != CHALLENGE_SIZE ||
        now - session.challengeCreatedAt >= CHALLENGE_LIFETIME_MS ||
        memcmp(plaintext, session.challenge, CHALLENGE_SIZE) != 0) return;
    this->promoteSession(session, counter);
  }
  // Data from a peer that has not proved itself is discarded.
}

void CommunicationService::handleData(SenderSession& session, uint64_t counter,
                                      uint8_t fragmentIndex, uint8_t fragmentCount,
                                      const uint8_t* plaintext,
                                      uint16_t plaintextLength) {
  if (fragmentCount == 0 || fragmentCount > MAX_FRAGMENT_COUNT ||
      fragmentIndex >= fragmentCount || counter < fragmentIndex) return;
  // Only the final fragment may be short, so every message has exactly one
  // valid framing and offsets always line up with the fragment lengths.
  if (fragmentIndex + 1 < fragmentCount && plaintextLength != MAX_FRAGMENT_SIZE) return;
  uint64_t messageId = counter - fragmentIndex;
  if (session.hasDeliveredMessage && messageId <= session.lastDeliveredMessageId) return;

  uint32_t now = millis();
  if (session.reassemblyActive && now - session.reassemblyStartedAt >= REASSEMBLY_TIMEOUT_MS)
    session.reassemblyActive = false;

  if (!session.reassemblyActive || session.reassemblyMessageId != messageId) {
    if (session.reassemblyActive && messageId < session.reassemblyMessageId) return;
    session.reassemblyActive = true;
    session.reassemblyMessageId = messageId;
    session.reassemblyStartedAt = now;
    session.reassemblyFragmentCount = fragmentCount;
    session.reassemblyMask = 0;
    memset(session.fragmentLengths, 0, sizeof(session.fragmentLengths));
  }
  if (session.reassemblyFragmentCount != fragmentCount ||
      (session.reassemblyMask & (1U << fragmentIndex)) != 0) return;

  size_t offset = static_cast<size_t>(fragmentIndex) * MAX_FRAGMENT_SIZE;
  if (offset + plaintextLength > MAX_PLAINTEXT_SIZE) {
    session.reassemblyActive = false;
    return;
  }
  memcpy(session.reassembly + offset, plaintext, plaintextLength);
  session.fragmentLengths[fragmentIndex] = plaintextLength;
  session.reassemblyMask |= 1U << fragmentIndex;

  uint8_t completeMask = (1U << fragmentCount) - 1;
  if (session.reassemblyMask == completeMask) this->deliverReassembly(session);
}

void CommunicationService::deliverReassembly(SenderSession& session) {
  size_t total = 0;
  for (uint8_t i = 0; i < session.reassemblyFragmentCount; ++i)
    total += session.fragmentLengths[i];
  if (total > MAX_PLAINTEXT_SIZE) {
    session.reassemblyActive = false;
    return;
  }

  PendingMessage message;
  message.senderNodeId = this->macToNodeId(session.mac);
  size_t length = 0;
  for (uint8_t i = 0; i < session.reassemblyFragmentCount; ++i) {
    size_t sourceOffset = static_cast<size_t>(i) * MAX_FRAGMENT_SIZE;
    uint16_t fragmentLength = session.fragmentLengths[i];
    memmove(message.payload + length, session.reassembly + sourceOffset, fragmentLength);
    length += fragmentLength;
  }
  message.length = static_cast<uint16_t>(length);
  message.payload[length] = '\0';
  if (xQueueSend(this->receiveQueue, &message, 0) == pdTRUE) {
    session.lastDeliveredMessageId = session.reassemblyMessageId;
    session.hasDeliveredMessage = true;
  } else {
    Serial.println("[WARN] Secure RX queue full, message dropped");
  }
  session.reassemblyActive = false;
}

bool CommunicationService::broadcast(String message, bool application) {
  if (!this->runtimeEnabled || !this->espNowInitialized || !this->keyValid) return false;
  size_t length = message.length();
  if (length > MAX_PLAINTEXT_SIZE) {
    Serial.printf("[ERROR] Secure message too large: %u bytes\n",
                  static_cast<unsigned>(length));
    return false;
  }

  uint8_t fragmentCount = static_cast<uint8_t>((length + MAX_FRAGMENT_SIZE - 1) /
                                                MAX_FRAGMENT_SIZE);
  if (fragmentCount == 0) fragmentCount = 1;
  if (uxQueueSpacesAvailable(this->sendQueue) < fragmentCount) {
    Serial.println("[WARN] Secure TX queue full, message dropped");
    return false;
  }
  for (uint8_t i = 0; i < fragmentCount; ++i) {
    size_t offset = static_cast<size_t>(i) * MAX_FRAGMENT_SIZE;
    uint16_t fragmentLength = static_cast<uint16_t>(min(length - offset, MAX_FRAGMENT_SIZE));
    if (!this->queueFrame(FrameType::DATA,
                          reinterpret_cast<const uint8_t*>(message.c_str()) + offset,
                          fragmentLength, i, fragmentCount, application)) return false;
  }
  return true;
}

bool CommunicationService::sendModeEvent(ModeEventKind kind, const String& id,
                                         const String& title, const String& version,
                                         uint16_t schemaVersion,
                                         const JsonDocument& payload,
                                         const String& command,
                                         const SyncVersion& syncVersion,
                                         uint32_t replyTo, bool application) {
  if (!this->isAvailable()) return false;
  JsonDocument event;
  event["type"] = MessageType::EVENT;
  event["message"]["eventKind"] = static_cast<uint8_t>(kind);
  event["message"]["mode"]["id"] = id;
  event["message"]["mode"]["title"] = title;
  event["message"]["mode"]["version"] = version;
  event["message"]["mode"]["schemaVersion"] = schemaVersion;
  event["message"]["sync"]["revision"] = syncVersion.revision;
  event["message"]["sync"]["origin"] = syncVersion.origin;
  if (replyTo != 0) event["message"]["sync"]["replyTo"] = replyTo;
  event["message"]["payload"] = payload;
  if (kind == ModeEventKind::COMMAND) event["message"]["command"] = command;
  String msg;
  serializeJson(event, msg);
  return this->broadcast(msg, application);
}

bool CommunicationService::sendStateEvent(const JsonDocument& state) {
  if (!this->applicationPublishing) return false;
  String id = state["id"].as<String>();
  String title = state["title"].as<String>();
  String version = state["version"].as<String>();
  uint16_t schemaVersion = state["schemaVersion"] | 0;
  JsonObjectConst registry = state["registry"].as<JsonObjectConst>();
  if (id.isEmpty() || title.isEmpty() || version.isEmpty() || schemaVersion == 0 ||
      registry.isNull()) {
    Serial.println("[ERROR] Invalid mode state, event not sent");
    return false;
  }
  JsonDocument payload;
  payload.set(registry);
  SyncVersion candidate(this->nextRevision(), this->localNodeId);
  if (!this->sendModeEvent(ModeEventKind::STATE, id, title, version, schemaVersion,
                           payload, "", candidate)) return false;
  this->syncClock = candidate.revision;
  this->currentSyncVersion = candidate;
  return true;
}

bool CommunicationService::sendStateSnapshot(const JsonDocument& state, uint32_t replyTo) {
  String id = state["id"].as<String>();
  String title = state["title"].as<String>();
  String version = state["version"].as<String>();
  uint16_t schemaVersion = state["schemaVersion"] | 0;
  JsonObjectConst registry = state["registry"].as<JsonObjectConst>();
  if (id.isEmpty() || title.isEmpty() || version.isEmpty() || schemaVersion == 0 ||
      registry.isNull()) return false;

  JsonDocument payload;
  payload.set(registry);
  SyncVersion current = this->currentSyncVersion;
  bool initialVersion = current.revision == 0;
  if (initialVersion) current = SyncVersion(this->nextRevision(), this->localNodeId);
  if (!this->sendModeEvent(ModeEventKind::STATE, id, title, version, schemaVersion,
                           payload, "", current, replyTo, false)) return false;
  if (initialVersion) {
    this->syncClock = current.revision;
    this->currentSyncVersion = current;
  }
  return true;
}

bool CommunicationService::sendModeCommand(const String& id, const String& title,
                                           const String& version, uint16_t schemaVersion,
                                           const String& command,
                                           const JsonDocument& payload) {
  if (!this->applicationPublishing) return false;
  // The document is const here, so it must be inspected through the const view;
  // is<JsonObject>() is always false on a const variant and would drop every
  // mode command.
  if (id.isEmpty() || title.isEmpty() || version.isEmpty() || schemaVersion == 0 ||
      command.isEmpty() ||
      !payload.is<JsonObjectConst>()) {
    Serial.println("[ERROR] Invalid mode command, event not sent");
    return false;
  }
  SyncVersion candidate(this->nextRevision(), this->localNodeId);
  if (!this->sendModeEvent(ModeEventKind::COMMAND, id, title, version, schemaVersion,
                           payload, command, candidate)) return false;
  this->syncClock = candidate.revision;
  this->currentSyncVersion = candidate;
  return true;
}

void CommunicationService::sendSyncRequest() {
  if (!this->isAvailable()) return;
  JsonDocument message;
  message["type"] = MessageType::SYNC;
  message["message"]["kind"] = "state.request";
  message["message"]["requester"] = this->localNodeId;
  String msg;
  serializeJson(message, msg);
  this->broadcast(msg, false);
}

void CommunicationService::sendWipe(uint16_t numberOfWipes) {
  if (!this->applicationPublishing) return;
  JsonDocument message;
  message["type"] = MessageType::WIPE;
  message["message"]["numberOfWipes"] = numberOfWipes;
  String msg;
  serializeJson(message, msg);
  this->broadcast(msg, true);
}

void CommunicationService::sendDistanceUpdate(uint16_t distance, uint16_t level) {
  if (!this->applicationPublishing) return;
  JsonDocument message;
  message["type"] = MessageType::LEVEL;
  message["message"]["distance"] = distance;
  message["message"]["level"] = level;
  message["message"]["active"] = true;
  String msg;
  serializeJson(message, msg);
  this->broadcast(msg, true);
}

uint32_t CommunicationService::macToNodeId(const uint8_t* mac) const {
  uint32_t id = 0;
  id |= static_cast<uint32_t>(mac[3]) << 24;
  id |= static_cast<uint32_t>(mac[4]) << 16;
  id |= static_cast<uint32_t>(mac[5]) << 8;
  id |= static_cast<uint8_t>(mac[0] ^ mac[1] ^ mac[2]);
  return id;
}

void CommunicationService::addNode(uint32_t id) {
  if (this->nodes.size() >= GLOW_MAX_GROUP_NODES || this->nodeExists(id)) return;
  this->nodes.add({id, millis()});
}

int16_t CommunicationService::getNode(uint32_t id, GlowNode* node) {
  for (int i = 0; i < this->nodes.size(); ++i) {
    if (this->nodes.get(i).id == id) {
      *node = this->nodes.get(i);
      return i;
    }
  }
  return -1;
}

void CommunicationService::removeNode(uint32_t id) {
  for (int i = 0; i < this->nodes.size(); ++i) {
    if (this->nodes.get(i).id == id) {
      this->nodes.remove(i);
      return;
    }
  }
}

void CommunicationService::removeOldNodes() {
  for (int i = 0; i < this->nodes.size(); ++i) {
    if (millis() - this->nodes.get(i).lastSeen > GLOW_NODE_TIMEOUT) this->nodes.remove(i--);
  }
}

bool CommunicationService::updateNode(uint32_t id) {
  GlowNode node;
  int16_t index = this->getNode(id, &node);
  if (index >= 0) {
    node.lastSeen = millis();
    this->nodes.set(index, node);
    return true;
  }
  this->addNode(id);
  return false;
}

bool CommunicationService::nodeExists(uint32_t id) {
  GlowNode node;
  return this->getNode(id, &node) >= 0;
}

void CommunicationService::receivedCallback(uint32_t from, String& msg) {
  if (from == this->localNodeId) return;
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.println("[ERROR] Invalid authenticated JSON message");
    return;
  }
  if (!doc["type"].is<int>()) return;
  int rawType = doc["type"].as<int>();
  if (rawType < 0 || rawType >= static_cast<int>(MessageType::MAX)) return;
  MessageType type = static_cast<MessageType>(rawType);
  if (type == MessageType::HEARTBEAT) return;
  if (this->receivedControllerCallback != nullptr)
    this->receivedControllerCallback(from, doc["message"], type);
}

const ArrayList<GlowNode>& CommunicationService::getNodes() const { return this->nodes; }

uint32_t CommunicationService::getNodeId() { return this->localNodeId; }

uint32_t CommunicationService::getMeshTime() { return millis(); }

bool CommunicationService::onNewConnection(std::function<void(uint32_t)> callback) {
  this->connectionCallback = callback;
  return true;
}

bool CommunicationService::onReceived(
    std::function<void(uint32_t, JsonDocument, MessageType)> callback) {
  this->receivedControllerCallback = callback;
  return true;
}

#ifdef GLOW_INTEGRATION_TEST

void CommunicationService::injectRawFrame(const uint8_t* sourceMac, const uint8_t* data,
                                          int length) {
  CommunicationService::onDataRecv(sourceMac, data, length);
}

void CommunicationService::printIdentity() const {
  Serial.print("[TEST] IDENTITY|");
  for (size_t i = 0; i < MAC_SIZE; ++i) Serial.printf("%02x", this->localMac[i]);
  Serial.print("|");
  for (size_t i = 0; i < BOOT_ID_SIZE; ++i) Serial.printf("%02x", this->bootId[i]);
  Serial.printf("|%u\n", this->localNodeId);
}

void CommunicationService::printNodes() const {
  Serial.print("[TEST] NODES|");
  for (size_t i = 0; i < this->nodes.size(); ++i) {
    if (i > 0) Serial.print(",");
    Serial.printf("%u", this->nodes.get(i).id);
  }
  Serial.println("");
}

#endif
