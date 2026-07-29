#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>
#include <ArrayList.h>

#include "AbstractMode.h"
#include "Alert.h"

#include "DistanceService.h"
#include "CommunicationService.h"

#ifndef GLOW_SYNC_FOLLOW_DEFAULT
#define GLOW_SYNC_FOLLOW_DEFAULT true
#endif

#ifndef GLOW_SYNC_PUBLISH_DEFAULT
#define GLOW_SYNC_PUBLISH_DEFAULT true
#endif

#ifndef GLOW_PORTAL_ENABLED
#define GLOW_PORTAL_ENABLED false
#endif

#ifndef GLOW_OTA_ENABLED
#define GLOW_OTA_ENABLED false
#endif

#ifdef GLOW_UNIT_TEST
struct GlowControllerTestAccess;
#endif

class Controller {
  private:
#ifdef GLOW_UNIT_TEST
    friend struct GlowControllerTestAccess;
#endif
    enum class SyncStatus : uint8_t {
      Unavailable,
      Detached,
      Joining,
      Synchronized,
    };

    ArrayList<AbstractMode*> modes;
    Alert* alertMode = nullptr;

    uint8_t currentModeIndex = 0;
    AbstractMode* currentMode = nullptr;
    AbstractMode* previousMode = nullptr;

    DistanceService* distanceService;
    CommunicationService* communicationService;

    bool levelUpdatePending = false;
    uint64_t lastLevelUpdate = 0;
    uint64_t lastConnectionAlert = 0;
    bool connectionAlertShown = false;
    bool syncFollow = GLOW_SYNC_FOLLOW_DEFAULT;
    bool syncPublish = GLOW_SYNC_PUBLISH_DEFAULT;
    bool syncFollowDefault = GLOW_SYNC_FOLLOW_DEFAULT;
    bool syncPublishDefault = GLOW_SYNC_PUBLISH_DEFAULT;
    bool captivePortalSupported = GLOW_PORTAL_ENABLED;
    bool otaEnabled = GLOW_OTA_ENABLED;
    bool localDirty = false;
    SyncStatus syncStatus = SyncStatus::Unavailable;
    uint32_t joinStartedAt = 0;
    uint32_t lastSyncRequestAt = 0;
    static constexpr uint32_t CONNECTION_ALERT_COOLDOWN = 30000;
    static constexpr uint32_t JOIN_TIMEOUT_MS = 2000;
    static constexpr uint32_t SYNC_REQUEST_INTERVAL_MS = 30000;

    void enableAlert(uint8_t flashes, CRGB color);
    void enableAlert(uint8_t flashes);
    void disableAlert();
    bool alertEnabled();

    void printSwitchedMode(AbstractMode* mode);
    AbstractMode* findMode(const String& title);
    AbstractMode* findModeById(const String& id);
    AbstractMode* synchronizedMode();
    int16_t findModeIndex(AbstractMode* mode);
    bool transitionTo(AbstractMode* mode, bool stateRestored = false);
    void synchronizeDistance();
    void maintainSync();
    void beginRejoin();
    void updatePublishingGate();
    bool canPublish() const;
    bool syncVersionIsNewer(const SyncVersion& candidate) const;
    const char* syncStatusName() const;
    void addSyncState(JsonObject target) const;
    void markLocalChange();
    bool publishCurrentState();
    void handleSyncMessage(const JsonDocument& message);

    bool event();

    void newConnectionCallback(uint32_t nodeId);
    void newMessageCallback(uint32_t from, JsonDocument doc, MessageType type);

  public:
    Controller(DistanceService* distanceService, CommunicationService* communicationService);

    void setAlertMode(Alert* mode);

    void addMode(AbstractMode* mode);
    void nextMode();
    void setMode(String title);

    void nextOption();
    void setOption(uint8_t option);
    void customClick();

    String getCurrentModeTitle();
    String getCurrentModeId();
    uint8_t getCurrentOption();

    JsonDocument capabilities();
    JsonDocument state();
    JsonDocument executeControl(const JsonDocument& request);
    void configureSyncDefaults(bool follow, bool publish);
    void configureRuntimeFeatures(bool captivePortal, bool ota);
    void requestResync();

    void setup();
    void loop();
};

#endif
