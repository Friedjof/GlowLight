#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>

#include "GlowRegistry.h"
#include "LightService.h"
#include "DistanceService.h"
#include "ButtonService.h"
#include "LinkService.h"

#include "GlowTypes.h"
#include "GlowConfig.h"


class Controller {
  private:
    GlowRegistry* registry;
    LightService* lightService;
    DistanceService* distanceService;
    ButtonService* buttonService;
    LinkService* linkService;

    uint64_t lastSequence = 0;
    bool initSequenceInitialized = false;
    bool initSequenceCompleted = false;
    uint8_t initSequenceCycles = 0;
    int8_t initSequenceDirection = 1;

    void initSequences();

  public:
    Controller(GlowRegistry* registry, LightService* lightService, DistanceService* distanceService, ButtonService* buttonService, LinkService* linkService);
    ~Controller();

    void setup();
    void loop();

    void receive(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    String command(JsonDocument doc);

    void simpleClickHandler();
    void doubleClickHandler();
    void longClickHandler();
};

#endif // CONTROLLER_H
