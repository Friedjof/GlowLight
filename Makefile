# Makefile für PlatformIO (ESP32-C3, Ubuntu /dev/ttyACM<N>)

PLATFORMIO ?= pio
BOARD ?= esp32c3

# Optionales "Argument" nach flash/monitor/run (z. B. "make flash 1")
ACTION_TARGETS := flash monitor run
ifneq ($(filter $(ACTION_TARGETS),$(firstword $(MAKECMDGOALS))),)
  ARG := $(word 2,$(MAKECMDGOALS))
  ifneq ($(ARG),)
    NR := $(ARG)
    # Dummy-Ziel erzeugen, damit die Zahl (z. B. "1") kein echtes Target ist
    $(eval $(ARG):;@:)
  endif
endif

# Optionale Flags je nach NR
ifdef NR
  UPLOAD_FLAG := --upload-port /dev/ttyACM$(NR)
  MONITOR_FLAG := --port /dev/ttyACM$(NR)
else
  UPLOAD_FLAG :=
  MONITOR_FLAG :=
endif

.PHONY: all build build-all build-profiles flash monitor run clean list \
        test test-native test-setup test-integration test-isolation test-security \
        test-ota test-homeassistant flash-all

all: build

build:
	$(PLATFORMIO) run --environment $(BOARD)

build-all:
	$(PLATFORMIO) run --environment esp32c3-all-modes

# Builds every profile, so a change cannot break one of them unnoticed.
build-profiles:
	$(PLATFORMIO) run --environment esp32c3
	$(PLATFORMIO) run --environment esp32c3-all-modes
	$(PLATFORMIO) run --environment esp32c3-integration
	python3 scripts/check_ota_layout.py --partitions partitions.csv \
		--firmware .pio/build/esp32c3/firmware.bin \
		--firmware .pio/build/esp32c3-all-modes/firmware.bin \
		--firmware .pio/build/esp32c3-integration/firmware.bin

# Everything that runs without hardware attached.
test: test-native test-setup

# Secure transport protocol tests: runs the real CommunicationService on the
# host. Pass FILTER=<substring> to run a subset.
test-native:
	$(MAKE) -C test/native test

# Setup, group key handling and host-side frame format.
test-setup:
	python3 scripts/setup/tests/group_key_test.py
	python3 test/hil/glow_frames_test.py

test-integration:
	python3 test/hil/two_lamp_sync.py

# Two lamps with different group keys must ignore each other. Reflashes both
# lamps and restores include/GlowConfig.h and the original key afterwards.
test-isolation:
	python3 test/hil/two_lamp_group_isolation.py

# Single-lamp security checks on real hardware. Needs the group key of the
# flashed lamp: make test-security KEY=<64 hex chars>
test-security:
	python3 test/hil/secure_transport.py --key $(KEY)

# Authenticated firmware update against a lamp on the infrastructure WiFi:
# make test-ota PORT=/dev/ttyACM0 HOST=glowlight.local
# The lamp's OTA password comes from the GLOW_OTA_PASSWORD environment variable.
test-ota:
	python3 test/hil/ota_update.py --port $(PORT) --host $(HOST)

# Home Assistant integration against a real broker. Needs paho-mqtt:
# make test-homeassistant BROKER=mqtt.local DEVICE=glow-1384610827 PORT=/dev/ttyACM0
test-homeassistant:
	python3 test/hil/home_assistant.py --broker $(BROKER) --device $(DEVICE) \
		--port $(PORT) $(if $(PEER_PORT),--peer-port $(PEER_PORT),)

# Flashes every connected lamp with the same image, which is what puts them
# into one group: the group is defined by the shared key in include/GlowConfig.h.
flash-all:
	@ports=$$(python3 -c "from serial.tools import list_ports; \
	print(' '.join(sorted(p.device for p in list_ports.comports() \
	if p.device.startswith('/dev/ttyACM') and p.vid == 0x303A)))"); \
	if [ -z "$$ports" ]; then echo "No lamps found"; exit 1; fi; \
	for port in $$ports; do \
		echo "==> $$port"; \
		$(PLATFORMIO) run --target upload --environment $(BOARD) --upload-port $$port || exit 1; \
	done; \
	echo "Flashed: $$ports"

# make flash        -> ohne --upload-port (auto-detect)
# make flash 1      -> Upload auf /dev/ttyACM1
flash:
	$(PLATFORMIO) run --target upload --environment $(BOARD) $(UPLOAD_FLAG)

# make monitor      -> ohne --port (auto-detect)
# make monitor 2    -> Monitor auf /dev/ttyACM2
monitor:
	$(PLATFORMIO) device monitor --environment $(BOARD) $(MONITOR_FLAG)

# make run          -> flash danach monitor (ohne Port)
# make run 1        -> flash/monitor auf /dev/ttyACM1
run: flash monitor

clean:
	$(PLATFORMIO) run --target clean --environment $(BOARD)

# make list         -> nur ESP-Geräte auf /dev/ttyACM<N> mit Nummern (ohne Duplikate)
list:
	@echo "NR  PORT          DESCRIPTION"
	@echo "--- ------------- --------------------------------------------------"
	@$(PLATFORMIO) device list --json-output | jq -r 'map(select(((.hwid // "") | test("VID:PID=303A:", "i")) or ((.description // "") | test("Espressif|USB JTAG/serial", "i")))) | map(select(.port | test("^/dev/ttyACM[0-9]+"))) | unique_by(.port) | .[] | (.port | capture("ACM(?<n>[0-9]+)").n) + "   " + .port + "  " + (.description // "")'
