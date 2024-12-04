PLATFORMIO = ~/.platformio/penv/bin/platformio
BOARD = esp32c3
BAND = 115200

all: build

build:
	$(PLATFORMIO) run --environment $(BOARD)

upload:
	$(PLATFORMIO) run --target upload --environment $(BOARD)

monitor:
	$(PLATFORMIO) device monitor --environment $(BOARD) --baud $(BAND)

clean:
	$(PLATFORMIO) run --target clean --environment $(BOARD)

flash: build upload monitor

start: clean flash

.PHONY: all build upload monitor clean flash start