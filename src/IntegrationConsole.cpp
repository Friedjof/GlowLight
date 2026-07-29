#include "IntegrationConsole.h"

#include <Arduino.h>

#include "CommunicationService.h"
#include "Controller.h"

#ifdef GLOW_INTEGRATION_TEST
namespace {

// CONTROL requests are larger than raw ESP-NOW frame injection commands.
constexpr size_t MAX_COMMAND_LENGTH = 2048;

int hexNibble(char character) {
  if (character >= '0' && character <= '9') return character - '0';
  if (character >= 'a' && character <= 'f') return character - 'a' + 10;
  if (character >= 'A' && character <= 'F') return character - 'A' + 10;
  return -1;
}

// Feeds a raw frame into the receive path so a single lamp can be tested
// against replayed, tampered or foreign-key traffic without a second radio.
void handleInject(CommunicationService& communicationService, const String& hex) {
  size_t length = hex.length() / 2;
  if (hex.length() % 2 != 0 || length == 0 || length > ESP_NOW_MAX_DATA_LEN) {
    Serial.println("[TEST] ERROR|INJECT|LENGTH");
    return;
  }

  static uint8_t frame[ESP_NOW_MAX_DATA_LEN];
  for (size_t i = 0; i < length; ++i) {
    int high = hexNibble(hex[i * 2]);
    int low = hexNibble(hex[i * 2 + 1]);
    if (high < 0 || low < 0) {
      Serial.println("[TEST] ERROR|INJECT|HEX");
      return;
    }
    frame[i] = static_cast<uint8_t>((high << 4) | low);
  }

  // The radio reports the sender address; frames carry it in the header too.
  uint8_t sourceMac[6] = {};
  if (length >= 22) memcpy(sourceMac, frame + 16, sizeof(sourceMac));

  communicationService.injectRawFrame(sourceMac, frame, static_cast<int>(length));
  Serial.printf("[TEST] OK|INJECT|%u\n", static_cast<unsigned>(length));
}

void printJsonResponse(const char* label, const JsonDocument& document) {
  Serial.printf("[TEST] %s|", label);
  serializeJson(document, Serial);
  Serial.println();
}

void handleControl(Controller& controller, const String& json) {
  JsonDocument request;
  DeserializationError error = deserializeJson(request, json);
  if (error) {
    Serial.printf("[TEST] ERROR|CONTROL_JSON|%s\n", error.c_str());
    return;
  }
  printJsonResponse("CONTROL", controller.executeControl(request));
}

}  // namespace
#endif

void integrationConsoleLoop(Controller& controller,
                            CommunicationService& communicationService) {
#ifdef GLOW_INTEGRATION_TEST
  static String command;
  static bool overflowed = false;

  while (Serial.available() > 0) {
    char input = Serial.read();

    if (input == '\n') {
      command.trim();

      // A truncated line must be reported, not silently misinterpreted.
      if (overflowed) {
        Serial.println("[TEST] ERROR|OVERFLOW");
        command = "";
        overflowed = false;
        continue;
      }

      if (command == "PING") {
        Serial.println("[TEST] PONG");
      } else if (command == "STATUS") {
        String title = controller.getCurrentModeTitle();
        Serial.printf("[TEST] STATUS|%s|%u\n", title.c_str(), controller.getCurrentOption());
      } else if (command == "CAPABILITIES") {
        printJsonResponse("CAPABILITIES", controller.capabilities());
      } else if (command == "STATE") {
        printJsonResponse("STATE", controller.state());
      } else if (command.startsWith("CONTROL ")) {
        handleControl(controller, command.substring(8));
      } else if (command == "NEXT_MODE") {
        controller.nextMode();
        Serial.println("[TEST] OK|NEXT_MODE");
      } else if (command == "NEXT_OPTION") {
        controller.nextOption();
        Serial.println("[TEST] OK|NEXT_OPTION");
      } else if (command == "IDENTITY") {
        communicationService.printIdentity();
      } else if (command == "NODES") {
        communicationService.printNodes();
      } else if (command == "TRACE ON" || command == "TRACE OFF") {
        communicationService.setFrameTrace(command.endsWith("ON"));
        Serial.printf("[TEST] OK|%s\n", command.c_str());
      } else if (command.startsWith("INJECT ")) {
        handleInject(communicationService, command.substring(7));
      } else if (!command.isEmpty()) {
        Serial.printf("[TEST] ERROR|UNKNOWN_COMMAND|%s\n", command.c_str());
      }

      command = "";
    } else if (input != '\r') {
      if (command.length() < MAX_COMMAND_LENGTH) {
        command += input;
      } else {
        overflowed = true;
      }
    }
  }
#endif
}
