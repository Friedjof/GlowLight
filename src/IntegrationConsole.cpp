#include "IntegrationConsole.h"

#include <Arduino.h>

#include "Controller.h"

void integrationConsoleLoop(Controller& controller) {
#ifdef GLOW_INTEGRATION_TEST
  static String command;

  while (Serial.available() > 0) {
    char input = Serial.read();

    if (input == '\n') {
      command.trim();

      if (command == "PING") {
        Serial.println("[TEST] PONG");
      } else if (command == "STATUS") {
        String title = controller.getCurrentModeTitle();
        Serial.printf("[TEST] STATUS|%s|%u\n", title.c_str(), controller.getCurrentOption());
      } else if (command == "NEXT_MODE") {
        controller.nextMode();
        Serial.println("[TEST] OK|NEXT_MODE");
      } else if (command == "NEXT_OPTION") {
        controller.nextOption();
        Serial.println("[TEST] OK|NEXT_OPTION");
      } else if (!command.isEmpty()) {
        Serial.printf("[TEST] ERROR|UNKNOWN_COMMAND|%s\n", command.c_str());
      }

      command = "";
    } else if (input != '\r' && command.length() < 64) {
      command += input;
    }
  }
#endif
}
