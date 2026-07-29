#ifndef INTEGRATIONCONSOLE_H
#define INTEGRATIONCONSOLE_H

class Controller;
class CommunicationService;

void integrationConsoleLoop(Controller& controller,
                            CommunicationService& communicationService);

#endif
