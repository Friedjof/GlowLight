PLATFORMIO = ~/.platformio/penv/bin/platformio
BOARD = esp32c3
BAND = 115200

PORT1 = /dev/ttyACM0
PORT2 = /dev/ttyACM1

all: build

setup:
	python scripts/setup.py

build:
	$(PLATFORMIO) run --environment $(BOARD)

upload: build
	$(PLATFORMIO) run --target upload --environment $(BOARD)

upload1: build
	$(PLATFORMIO) run --target upload --environment $(BOARD) --upload-port $(PORT1)

upload2: build
	$(PLATFORMIO) run --target upload --environment $(BOARD) --upload-port $(PORT2)

monitor:
	$(PLATFORMIO) device monitor --environment $(BOARD) --baud $(BAND)

monitor1:
	$(PLATFORMIO) device monitor --environment $(BOARD) --baud $(BAND) --port $(PORT1)

monitor2:
	$(PLATFORMIO) device monitor --environment $(BOARD) --baud $(BAND) --port $(PORT2)

clean:
	$(PLATFORMIO) run --target clean --environment $(BOARD)

run: upload monitor

flash: upload monitor

flash1: upload1 monitor1

flash2: upload2 monitor2

start: clean flash monitor

start1: clean flash1 monitor1

start2: clean flash2 monitor2

.PHONY: all build upload monitor clean flash start