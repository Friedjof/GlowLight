#ifndef LINK_SERVICE_H
#define LINK_SERVICE_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <map>

#include "GlowTypes.h"
#include "GlowConfig.h"


class LinkService {
  private:
    // Private members can be added here
    uint64_t lastHeartbeat = 0;

    String (*dataHandler)(JsonDocument doc) = nullptr;
    String (*commandHandler)(JsonDocument doc) = nullptr;

    std::map<std::array<uint8_t, 6>, PeerInfo> peers;
  public:
    LinkService();
    ~LinkService();

    uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    void setup();
    void loop();

    bool send(const uint8_t* peer_addr, String data, LinkMessageType type);
    void receive(const esp_now_recv_info_t* info, const uint8_t* data, int len);

    void addPeer(const uint8_t* peer_addr);
    void updatePeer(const uint8_t* peer_addr);
    void updatePeer(const uint8_t* peer_addr, uint64_t uptime);
    void removePeer(const uint8_t* peer_addr);
    bool isPeer(const uint8_t* peer_addr);
    void printPeers();
    std::map<std::array<uint8_t, 6>, PeerInfo> getPeers();

    void setDataHandler(String (*f)(JsonDocument doc));
    void setCommandHandler(String (*f)(JsonDocument doc));
};

#endif // LINK_SERVICE_H