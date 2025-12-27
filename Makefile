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

.PHONY: all build flash monitor run clean list

all: build

build:
	$(PLATFORMIO) run --environment $(BOARD)

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
