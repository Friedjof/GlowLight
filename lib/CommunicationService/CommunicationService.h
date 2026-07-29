#ifndef COMMUNICATIONSERVICE_H
#define COMMUNICATIONSERVICE_H

#include <Arduino.h>
#include <ArrayList.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <functional>

#include "GlowConfig.h"

#ifndef GLOW_MAX_GROUP_NODES
#define GLOW_MAX_GROUP_NODES 8
#endif

struct GlowNode {
  uint32_t id;
  uint64_t lastSeen;

  bool operator==(const GlowNode& other) const { return this->id == other.id; }
};

struct CommunicationConfig {
  bool enabled = MESH_ON;
  String groupKeyHex = GLOW_GROUP_KEY_HEX;
};

enum MessageType {
  EVENT = 0,
  SYNC = 1,
  HEARTBEAT = 2,
  WIPE = 3,
  LEVEL = 4,
  MAX
};

enum class ModeEventKind : uint8_t {
  STATE = 0,
  COMMAND = 1,
  MAX
};

struct SyncVersion {
  uint64_t revision;
  uint32_t origin;

  SyncVersion(uint64_t revision = 0, uint32_t origin = 0)
      : revision(revision), origin(origin) {}
};

#ifdef GLOW_UNIT_TEST
struct GlowSecureTestAccess;
#endif

class CommunicationService {
 private:
#ifdef GLOW_UNIT_TEST
  friend struct GlowSecureTestAccess;
#endif

  enum class FrameType : uint8_t {
    HELLO = 1,
    CHALLENGE = 2,
    PROOF = 3,
    HEARTBEAT = 4,
    DATA = 5
  };

  static constexpr size_t MAC_SIZE = 6;
  static constexpr size_t KEY_SIZE = 32;
  static constexpr size_t BOOT_ID_SIZE = 16;
  static constexpr size_t CHALLENGE_SIZE = 16;
  static constexpr size_t GROUP_TAG_SIZE = 8;
  static constexpr size_t GCM_TAG_SIZE = 16;
  static constexpr size_t NONCE_SIZE = 12;
  static constexpr size_t HEADER_SIZE = 48;
  static constexpr size_t MAX_PLAINTEXT_SIZE = 512;
  static constexpr size_t MAX_FRAGMENT_COUNT = 3;
  static constexpr size_t MAX_FRAGMENT_SIZE = ESP_NOW_MAX_DATA_LEN - HEADER_SIZE - GCM_TAG_SIZE;
  static constexpr uint8_t RECEIVE_QUEUE_LENGTH = 8;
  static constexpr uint8_t SEND_QUEUE_LENGTH = 16;
  static constexpr uint32_t CHALLENGE_RETRY_MS = 750;
  static constexpr uint32_t CHALLENGE_LIFETIME_MS = 5000;
  static constexpr uint32_t REASSEMBLY_TIMEOUT_MS = 2000;
  static constexpr uint32_t UNKNOWN_CHALLENGE_INTERVAL_MS = 750;
  static constexpr uint8_t MAX_CHALLENGE_RETRIES = 4;
  // Releases the transmit path if the driver never reports a send completion.
  static constexpr uint32_t SEND_TIMEOUT_MS = 100;
  // The sync clock only ever counts state changes, and it lives in RAM, so it is
  // bounded by uptime. A peer reporting a revision further ahead than this is
  // reporting nonsense; adopting it could push the clock towards the point where
  // the next increment wraps to zero and this lamp would lose every comparison
  // from then on.
  static constexpr uint64_t MAX_SYNC_ADVANCE = 1000000;

  static_assert(GLOW_MAX_GROUP_NODES > 0 && GLOW_MAX_GROUP_NODES <= 8,
                "GLOW_MAX_GROUP_NODES must be between 1 and 8");
  static_assert(MAX_FRAGMENT_SIZE * MAX_FRAGMENT_COUNT >= MAX_PLAINTEXT_SIZE,
                "ESP-NOW frames cannot carry the configured plaintext limit");
  static_assert(HEADER_SIZE + MAX_FRAGMENT_SIZE + GCM_TAG_SIZE <= ESP_NOW_MAX_DATA_LEN,
                "secure frame exceeds ESP-NOW MTU");
  static_assert(2 + 1 + 1 + 1 + 1 + 1 + 1 + GROUP_TAG_SIZE + MAC_SIZE +
                    BOOT_ID_SIZE + 8 + 2 == HEADER_SIZE,
                "serialized secure header size changed");

  struct RawFrame {
    uint8_t sourceMac[MAC_SIZE];
    uint16_t length;
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
  };

  struct PendingMessage {
    uint32_t senderNodeId;
    uint16_t length;
    char payload[MAX_PLAINTEXT_SIZE + 1];
  };

  struct PendingTransmission {
    uint16_t length;
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
    bool application;
  };

  // One slot per peer MAC. A slot holds at most one authenticated boot session
  // and, independently, one boot session that is currently being challenged.
  // The authenticated session is only ever replaced by a challenge that has
  // been answered with a fresh proof, so replayed traffic cannot unseat a peer.
  struct SenderSession {
    bool used;
    uint8_t mac[MAC_SIZE];
    uint32_t lastActivityAt;

    bool established;
    uint8_t bootId[BOOT_ID_SIZE];
    uint8_t key[KEY_SIZE];
    uint64_t replayMax;
    uint64_t replayBitmap;

    bool challengePending;
    uint8_t pendingBootId[BOOT_ID_SIZE];
    uint8_t pendingKey[KEY_SIZE];
    uint64_t pendingMaxCounter;
    uint8_t challenge[CHALLENGE_SIZE];
    uint32_t challengeCreatedAt;
    uint32_t challengeSentAt;
    uint32_t lastChallengeRequestAt;
    uint8_t challengeRetries;

    uint64_t lastDeliveredMessageId;
    bool hasDeliveredMessage;
    bool reassemblyActive;
    uint64_t reassemblyMessageId;
    uint32_t reassemblyStartedAt;
    uint8_t reassemblyFragmentCount;
    uint8_t reassemblyMask;
    uint16_t fragmentLengths[MAX_FRAGMENT_COUNT];
    uint8_t reassembly[MAX_PLAINTEXT_SIZE];
  };

  uint8_t localMac[MAC_SIZE] = {};
  uint8_t groupKey[KEY_SIZE] = {};
  uint8_t groupTag[GROUP_TAG_SIZE] = {};
  uint8_t bootId[BOOT_ID_SIZE] = {};
  uint8_t localKey[KEY_SIZE] = {};
  uint32_t localNodeId = 0;
  uint64_t sendCounter = 0;
  bool espNowInitialized = false;
  bool keyValid = false;
  bool runtimeEnabled = MESH_ON;

  static CommunicationService* instance;
  QueueHandle_t rawReceiveQueue = nullptr;
  QueueHandle_t receiveQueue = nullptr;
  QueueHandle_t sendQueue = nullptr;
  PendingTransmission activeTransmission = {};
  std::atomic<bool> sendInProgress{false};
  uint32_t sendStartedAt = 0;
#ifdef GLOW_INTEGRATION_TEST
  bool traceFrames = false;
#endif

  std::function<void(uint32_t)> connectionCallback = nullptr;
  std::function<void(uint32_t, JsonDocument, MessageType)> receivedControllerCallback = nullptr;
  ArrayList<GlowNode> nodes;
  SenderSession sessions[GLOW_MAX_GROUP_NODES] = {};
  uint64_t lastHeartbeat = 0;
  bool applicationPublishing = true;
  uint64_t syncClock = 0;
  SyncVersion currentSyncVersion;

  static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
  static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);

  bool parseGroupKey(const char* hex);
  bool deriveGroupTag();
  bool deriveBootKey(const uint8_t* mac, const uint8_t* remoteBootId, uint8_t* key) const;
  static bool hkdfSha256(const uint8_t* ikm, size_t ikmLength, const uint8_t* salt,
                         size_t saltLength, const uint8_t* info, size_t infoLength,
                         uint8_t* output, size_t outputLength);
  static void writeUint16(uint8_t* output, uint16_t value);
  static void writeUint64(uint8_t* output, uint64_t value);
  static uint16_t readUint16(const uint8_t* input);
  static uint64_t readUint64(const uint8_t* input);
  static void buildNonce(uint64_t counter, uint8_t* nonce);

  bool queueFrame(FrameType type, const uint8_t* plaintext, uint16_t plaintextLength,
                  uint8_t fragmentIndex = 0, uint8_t fragmentCount = 1,
                  bool application = false);
  bool encryptFrame(FrameType type, uint64_t counter, const uint8_t* plaintext,
                    uint16_t plaintextLength, uint8_t fragmentIndex,
                    uint8_t fragmentCount, PendingTransmission* transmission);
  void processRawFrame(const RawFrame& raw);
  void processSendQueue();
  void processPendingChallenges();
  void sendHello();
  void sendChallenge(SenderSession& session);
  void sendProof(const uint8_t* challenge);
  void handleEstablishedFrame(SenderSession& session, FrameType type, uint64_t counter,
                              uint8_t fragmentIndex, uint8_t fragmentCount,
                              const uint8_t* plaintext, uint16_t plaintextLength);
  void handleCandidateFrame(SenderSession& session, const uint8_t* remoteBootId,
                            const uint8_t* key, FrameType type, uint64_t counter,
                            const uint8_t* plaintext, uint16_t plaintextLength);
  void answerChallenge(const uint8_t* plaintext, uint16_t plaintextLength);

  SenderSession* findSession(const uint8_t* mac);
  SenderSession* acquireSession(const uint8_t* mac);
  void beginChallenge(SenderSession& session, const uint8_t* remoteBootId,
                      const uint8_t* key, uint64_t counter);
  bool replayAcceptable(const SenderSession& session, uint64_t counter) const;
  void commitReplay(SenderSession& session, uint64_t counter);
  void promoteSession(SenderSession& session, uint64_t proofCounter);
  void handleData(SenderSession& session, uint64_t counter, uint8_t fragmentIndex,
                  uint8_t fragmentCount, const uint8_t* plaintext,
                  uint16_t plaintextLength);
  void deliverReassembly(SenderSession& session);

  // Saturates instead of wrapping, so a revision can never fall back to zero.
  uint64_t nextRevision() const;

  uint32_t macToNodeId(const uint8_t* mac) const;
  void receivedCallback(uint32_t from, String& msg);
  bool broadcast(String message, bool application = false);
  bool sendModeEvent(ModeEventKind kind, const String& id, const String& title,
                     const String& version, uint16_t schemaVersion,
                     const JsonDocument& payload, const String& command,
                     const SyncVersion& syncVersion, uint32_t replyTo = 0,
                     bool application = true);
  void addNode(uint32_t id);
  int16_t getNode(uint32_t id, GlowNode* node);
  void removeNode(uint32_t id);
  bool updateNode(uint32_t id);
  void removeOldNodes();
  bool nodeExists(uint32_t id);

 public:
  CommunicationService();
  ~CommunicationService();

  void setup();
  void setup(const CommunicationConfig& config);
  void loop();
  bool isAvailable() const;
  bool isConfigured() const { return this->runtimeEnabled; }
  bool setApplicationPublishing(bool enabled);
  bool isApplicationPublishing() const { return this->applicationPublishing; }
  SyncVersion syncVersion() const { return this->currentSyncVersion; }
  void acceptSyncVersion(const SyncVersion& version);
  void radioChannelChanged(uint8_t channel);
  const ArrayList<GlowNode>& getNodes() const;
  bool sendStateEvent(const JsonDocument& state);
  bool sendStateSnapshot(const JsonDocument& state, uint32_t replyTo);
  bool sendModeCommand(const String& id, const String& title, const String& version,
                       uint16_t schemaVersion, const String& command,
                       const JsonDocument& payload);
  void sendSyncRequest();
  void sendWipe(uint16_t numberOfWipes);
  void sendDistanceUpdate(uint16_t distance, uint16_t level);
  uint32_t getNodeId();
  uint32_t getMeshTime();
  bool onNewConnection(std::function<void(uint32_t)> callback);
  bool onReceived(std::function<void(uint32_t, JsonDocument, MessageType)> callback);

#ifdef GLOW_INTEGRATION_TEST
  // Hardware test hooks. These exist only in the esp32c3-integration profile
  // and are compiled out of every production build.
  void injectRawFrame(const uint8_t* sourceMac, const uint8_t* data, int length);
  void printIdentity() const;
  void printNodes() const;
  // Echoes every transmitted frame as hex so a host can follow the handshake
  // without a second radio. Off unless a test turns it on.
  void setFrameTrace(bool enabled) { this->traceFrames = enabled; }
#endif
};

#endif
