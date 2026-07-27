#ifndef MODEREGISTRATION_H
#define MODEREGISTRATION_H

class CommunicationService;
class Controller;
class DistanceService;
class LightService;

void registerModes(Controller& controller, LightService& lightService,
                   DistanceService& distanceService,
                   CommunicationService& communicationService);

#endif
