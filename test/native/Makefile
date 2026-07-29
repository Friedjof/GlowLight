# Native (host) test suite for the GlowLight secure ESP-NOW transport.
#
# Compiles the real lib/CommunicationService/CommunicationService.cpp against
# thin shims for Arduino, ESP-NOW, FreeRTOS and mbedTLS (mapped onto OpenSSL),
# so the protocol logic under test is the firmware's own.

PROJECT_ROOT := $(abspath $(CURDIR)/../..)
LIBDEPS := $(PROJECT_ROOT)/.pio/libdeps/esp32c3
BUILD := $(CURDIR)/build

CXX ?= g++
CXXFLAGS := -std=gnu++17 -g -O1 -Wall -Wextra -Wno-unused-parameter -MMD -MP \
            -DGLOW_UNIT_TEST=1 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1
INCLUDES := -I$(CURDIR) -I$(CURDIR)/shims \
            -I$(PROJECT_ROOT)/lib/CommunicationService \
             -I$(PROJECT_ROOT)/lib/NetworkService \
            -I$(PROJECT_ROOT)/lib/HomeAssistantService \
             -I$(PROJECT_ROOT)/lib/ConfigService \
             -I$(PROJECT_ROOT)/lib/OtaService \
            -I$(PROJECT_ROOT)/lib/GlowRegistry \
            -I$(PROJECT_ROOT)/lib/AbstractMode \
            -I$(PROJECT_ROOT)/lib/Controller \
            -I$(PROJECT_ROOT)/lib/Alert \
            -I$(PROJECT_ROOT)/lib/LightService \
            -I$(PROJECT_ROOT)/lib/DistanceService \
            -I$(LIBDEPS)/ArduinoJson/src \
            -I$(LIBDEPS)/ArrayList/src
LDLIBS := -lcrypto

SOURCES := $(wildcard $(CURDIR)/*.cpp) \
           $(CURDIR)/shims/shims.cpp \
           $(PROJECT_ROOT)/lib/CommunicationService/CommunicationService.cpp \
           $(PROJECT_ROOT)/lib/NetworkService/NetworkService.cpp \
           $(PROJECT_ROOT)/lib/HomeAssistantService/HomeAssistantProtocol.cpp \
           $(PROJECT_ROOT)/lib/ConfigService/ConfigService.cpp \
           $(PROJECT_ROOT)/lib/ConfigService/PreferencesConfigStore.cpp \
           $(PROJECT_ROOT)/lib/OtaService/OtaService.cpp \
           $(PROJECT_ROOT)/lib/GlowRegistry/GlowRegistry.cpp \
           $(PROJECT_ROOT)/lib/AbstractMode/AbstractMode.cpp \
           $(PROJECT_ROOT)/lib/Controller/Controller.cpp
OBJECTS := $(patsubst $(PROJECT_ROOT)/%.cpp,$(BUILD)/%.o,$(SOURCES))
DEPENDENCIES := $(OBJECTS:.o=.d)
TARGET := $(BUILD)/glow_native_tests

.PHONY: all test clean

all: $(TARGET)

test: $(TARGET)
	@$(TARGET) $(FILTER)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: $(PROJECT_ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)

# Missing library sources are the usual cause of a failed build.
$(LIBDEPS)/ArduinoJson/src:
	@echo "ArduinoJson not found. Run 'pio pkg install -e esp32c3' first." >&2
	@false
