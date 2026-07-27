# 🌟 GlowLight - Smart Wireless Bedside Lamp

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: ESP32-C3](https://img.shields.io/badge/Platform-ESP32--C3-red.svg)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-cyan.svg)](https://www.arduino.cc/)

A beautiful, smart bedside lamp with ESP-NOW wireless communication, gesture controls, and multiple lighting modes. Built on ESP32-C3 with 3D-printed components and WS2812B LED strips.

## 📋 Table of Contents

- [✨ Features](#-features)
- [🎭 Available Lighting Modes](#-available-lighting-modes)
- [🚀 Quick Setup](#-quick-setup)
- [📸 Gallery](#-gallery)
- [🔗 Mesh Communication](#-mesh-communication-new)
- [🔧 Hardware Components](#-hardware-components)
  - [Main Components](#main-components)
  - [3D Printing](#3d-printing)
  - [Soldering](#soldering)
- [📦 Software Installation (Advanced)](#-software-installation-advanced)
- [👨‍💻 Development](#-development)
- [📄 License](#-license)

## ✨ Features

- **🎨 Multiple Lighting Modes**: Static colors, rainbow, beacon, candle effect, and more
- **⚡ ESP-NOW Communication**: Ultra-fast (<10ms) wireless synchronization between lamps
- **👋 Gesture Control**: Hand proximity sensing with VL53L0X distance sensor
- **🔘 Physical Controls**: Simple button interface for mode switching
- **🏠 3D Printable**: Complete STL files for custom lamp housing
- **🔧 Easy Setup**: One-command installation with interactive configuration
- **📱 Device Management**: Automatic ESP32 detection and flashing
- **💾 Backup System**: Configuration backup and restore

## 🎭 Available Lighting Modes

### Active Modes
- **🎨 StaticMode**: Single solid color with full brightness control
- **🌈 ColorPickerMode**: Cycle through color spectrum with distance sensor
- **🌈 RainbowMode**: Classic flowing rainbow effect with speed control
- **✨ RandomGlowMode**: 10-color random transitions with zen to party speeds
  - *NEW v3.3.0*: Enhanced with 10 scientifically distributed colors (every 36° in HSV space)
  - Features: Pause/transition cycles, inherited brightness control, mesh synchronization
  - Speed modes: Zen (meditative) → Normal → Lebendig (energetic) → Hektisch (party)

### Optional Modes
- **🕯️ CandleMode**: Realistic candle flicker simulation
- **🚨 BeaconMode**: Beacon/alert patterns for notifications
- **🌅 SunsetMode**: Natural sunset simulation for bedtime
- **💫 StrobeMode**: Synchronized strobe effects over ESP-NOW
- **🎮 MiniGame**: Interactive games using distance sensor

> **Note**: Mode selection is declared in `include/ModeConfig.h`. Set an individual
> `GLOW_ENABLE_*` switch to `1`, or use the `esp32c3-all-modes` build profile.

## 📸 Gallery

<table>
  <tr>
    <td><img src="media/images/components/assembled_lampshade.jpg" alt="Assembled Lampshade"></td>
    <td><img src="media/images/demo/rainbow_mode.jpg" alt="Rainbow Mode"></td>
    <td><img src="media/images/components/usb-c_port.jpg" alt="USB-C Port"></td>
  </tr>
</table>

[-> Rainbow Mode Demo Video](media/images/demo/dual_lamps_rainbow_mode.mp4)

![Modes](media/images/diagrams/modes.png)

This is an overview of the different modes available in the lamp. The modes can be toggled using the button.

## 🔗 ESP-NOW Wireless Communication

![Communication](media/images/diagrams/communication.png)

[-> Wireless Synchronization Demo Video](media/images/demo/3_lamps_communication.mp4)

GlowLight uses **ESP-NOW** for ultra-fast, low-latency wireless communication between lamps. ESP-NOW provides direct device-to-device communication without requiring a WiFi router, enabling instant synchronization across multiple lamps.

### Why ESP-NOW?

- ⚡ **Ultra-low latency**: <10ms message delivery (vs 50-200ms with traditional mesh)
- 🔋 **Energy efficient**: Minimal power consumption compared to WiFi mesh
- 📡 **Simple setup**: No SSID/password configuration needed - just power on
- 🎯 **Reliable**: Direct broadcast communication, no routing overhead
- 📶 **Good range**: Up to 200m line-of-sight
- 🔓 **No peer limits**: Supports unlimited number of lamps via broadcast

### Configuration

You can configure the WiFi channel in `include/GlowConfig.h`:

```cpp
#define ESPNOW_CHANNEL 1  // WiFi channel (1-13), configurable
```

**Note**: All lamps must be on the same WiFi channel to communicate. The default channel 1 works well in most environments.

### How It Works

1. **Automatic Discovery**: Lamps discover each other automatically via heartbeat messages (every 10 seconds)
2. **Instant Sync**: State changes (mode, color, brightness) are broadcast instantly to all lamps
3. **No Infrastructure**: No router, server, or internet connection required
4. **Self-Organizing**: New lamps automatically join the network when powered on
5. **Resilient**: 30-minute timeout removes inactive lamps from the network

For technical details, see the [CommunicationService README](lib/CommunicationService/README.md).

## 🔧 Hardware Components

- DUBEUYEW ESP32-C3 Development Board Mini
- VL53L0X distance sensor
- Simple push button (height ≥ 6mm)
- WS2812B 5V LED strip (11 LEDs)
- External 5V power supply
- USB-C and some other necessary cables
- 3x M3 threaded insert
- 3x M3 screws

### Main Components

![Main Components](media/images/components/button_sensor_esp32c3_led_mini.png)

### Tools and Materials

- 3D printer + filament (white and a color of your choice)
- Soldering iron + solder
- 2x Heat shrink tube
- Screwdriver

### 3D Printing

> You can find the 3D models in the [`/printing`](/printing) folder. The models are designed to be 3D printed and assembled. The lamp consists of three parts: the base, the lampshade, and the lampshade holder.

<table>
  <tr>
    <td><img src="media/images/printing/3d_printed_parts.jpg" alt="Printed Parts"></td>
    <td><img src="media/images/components/assembled_lamp_base.jpg" alt="Assembled Lamp Base"></td>
    <td><img src="media/images/components/lamp_final_assembly.jpg" alt="Final Assembly"></td>
  </tr>
</table>

### Soldering

The components are connected to the ESP32C3 using the following diagram:

![Soldering Diagram](media/images/components/soldering_diagram.png)

> The Button does not require a resistor, as the ESP32C3 has internal pull-up resistors.

This table also shows the connections:

| Component | Pin | ESP32C3 Pin |
| --------- | --- | ----------- |
| Button    | 1   | GND         |
|           | 2   | GPIO 4      |
| VL53L0X   | VCC | 5V          |
|           | GND | GND         |
|           | SDA | GPIO 6      |
|           | SCL | GPIO 7      |
| WS2812B   | VCC | 5V          |
|           | GND | GND         |
|           | DI  | GPIO 3      |

> The `VL53L0X` is the distance sensor, the `WS2812B` is the LED strip, and the `Button` is the push button.

<table>
  <tr>
    <td><img src="media/images/components/button_sensor_esp32c3_led.jpg" alt="Button, Sensor, ESP32C3, and LED"></td>
    <td><img src="media/images/components/wiring_setup_lamp.jpg" alt="Wiring Setup"></td>
  </tr>
</table>

### Threaded Insert

To attach the lampshade to the base, a threaded insert is used. The insert is placed in the base, and the lampshade is screwed onto it.

<table>
  <tr>
    <td><img src="media/images/printing/thread_insertion_soldering.jpg" alt="Thread Insertion Soldering"></td>
    <td><img src="media/images/printing/lid_attachment_screw.jpg" alt="Lid Attachment Screw"></td>
    <td><img src="media/images/components/bottom_side.jpg" alt="Bottom Side"></td>
  </tr>
</table>

## 🚀 Quick Setup

![Setup Script](media/images/scripts/setup-menu.png)

**Get started in just one command!** The GlowLight setup system will guide you through the entire installation process:

```bash
curl -fsSL https://raw.githubusercontent.com/friedjof/GlowLight/master/install.sh > install.sh && bash install.sh
```

**Alternative download method:**
```bash
wget https://raw.githubusercontent.com/friedjof/GlowLight/master/install.sh && bash install.sh
```

### What the installer does:

1. **🔍 Checks system requirements** (Python 3.8+, Git)
2. **📦 Installs dependencies** automatically for your OS (Ubuntu/Debian, Fedora, Arch, macOS)
3. **📂 Downloads the GlowLight project** to `~/GlowLight`
4. **🛠️ Launches the interactive setup system** with a beautiful menu interface

### Interactive Setup Features:

- **⚙️ Project Configuration**: Set up mesh network and GPIO pins with guided wizards
- **🔨 Build & Flash**: Compile and upload firmware to your ESP32-C3 with one click
- **📱 Device Management**: Automatic ESP32 device detection and management
- **📺 Serial Monitor**: Real-time device monitoring with logging
- **🔧 PlatformIO Setup**: Automatic PlatformIO installation and management
- **💾 Backup System**: Configuration backup and restore functionality

### Manual Installation

If you prefer to set up manually:

```bash
# Clone the repository
git clone https://github.com/friedjof/GlowLight.git
cd GlowLight

# Run the setup system
python3 scripts/setup.py
```

## 📦 Software Installation (Advanced)

This is a PlatformIO project. The setup system above handles everything automatically, but for manual installation, PlatformIO must be installed. Once installed, you can open the project in PlatformIO and flash the software onto the ESP32C3.

Alternatively, a `Makefile` is included, allowing you to flash the software via the command line. For this, PlatformIO must be installed, and the `PLATFORMIO` environment variable should point to the PlatformIO executable.

If you're familiar with Nix-shell, you can use the [`shell.nix`](/shell.nix) file to set up the environment for PlatformIO.

### PlatformIO Commands

- `pio run`: Compiles the software
- `pio run --environment esp32c3-all-modes`: Compiles and links every mode
- `pio run --target upload`: Flashes the software to the ESP32C3
- `pio run --target clean`: Removes compiled files
- `pio device monitor`: Opens a terminal to view the ESP32C3 output

### Makefile Commands

- `make` or `make build`: Compiles the standard profile
- `make build-all`: Compiles and links every mode
- `make flash [device-number]`: Flashes the software to the ESP32-C3
- `make clean`: Removes compiled files
- `make monitor [device-number]`: Opens the serial monitor
- `make run [device-number]`: Flashes the software and opens the monitor
- `make list`: Lists matching ESP32-C3 serial devices

### Libraries Used

- [`ArrayList`](https://registry.platformio.org/libraries/braydenanderson2014/ArrayList) for dynamic arrays
- [`Button2`](https://registry.platformio.org/libraries/lennarthennigs/Button2) for button input handling
- [`Adafruit_VL53L0X`](https://github.com/adafruit/Adafruit_VL53L0X) for the distance sensor
- [`FastLED`](https://registry.platformio.org/libraries/fastled/FastLED) for LED control
- [`ArduinoJson`](https://registry.platformio.org/libraries/bblanchon/ArduinoJson) for message serialization
- **ESP-NOW** (built-in ESP32 library) for wireless communication

For more details on the libraries, refer to the [`platformio.ini`](/platformio.ini) file.

## 👨‍💻 Development

The software is written in C++ and is structured as a typical PlatformIO project. The main file is [`src/main.cpp`](/src/main.cpp), which contains the setup and loop functions. The different modes, services, and the controller are implemented in separate files in the [`/lib`](/lib) folder.

### Technical Diagrams

For a comprehensive understanding of the GlowLight system architecture and operation, see the [Technical Diagrams](docs/diagrams.md) which includes:

- **[Class Diagram](docs/diagrams.md#class-diagram)**: Complete class structure and relationships
- **[Main Program Flow](docs/diagrams.md#main-program-flow)**: Startup sequence and main loop execution
- **[Mode System Flow](docs/diagrams.md#mode-system-flow)**: Lighting modes management and switching
- **[Service Integration Flow](docs/diagrams.md#service-integration-flow)**: LightService, DistanceService, and CommunicationService interaction
- **[Button Interaction Flow](docs/diagrams.md#button-interaction-flow)**: Button press handling and mode switching logic
- **[ESP-NOW Communication Flow](docs/diagrams.md#esp-now-communication-flow)**: Wireless networking and synchronization
- **[Available Modes Detail](docs/diagrams.md#available-modes-detail)**: Overview of all lighting modes and their features

### Modes

Every mode is a class that inherits from the `AbstractMode` class. The abstract class already implements the basic functions that every mode should have. In every mode, the following functions must be implemented: `setup`, `customFirst`, `customLoop`, `last`, and `customClick`.

- `setup`: This function is called once when the mode is added to the controller when the lamp is turned on.
- `customFirst`: This function is called once when the mode is newly selected.
- `customLoop`: This function is called every loop iteration.
- `last`: This function is called once when the mode is removed from the controller.
- `customClick`: This function is called when a double click is detected from the button.

## 📄 License

This project is licensed under the GNU General Public License v3.0. For more information, see the [`LICENSE`](/LICENSE) file.

---

## ⚠️ Beta Notice

This is a beta version of the project. The software is still under development, and the hardware may require some adjustments. I cannot guarantee that the project will work as expected and will not be responsible for any damage caused by the project.

## 🎯 Quick Start Reminder

**New to GlowLight?** Get started with just one command:

```bash
curl -fsSL https://raw.githubusercontent.com/friedjof/GlowLight/master/install.sh | bash
```

The setup system will guide you through everything! 🚀
