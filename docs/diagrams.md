# GlowLight - Technical Diagrams

This document contains all technical diagrams for the GlowLight Smart Mesh Bedside Lamp project, including class relationships and program flow visualizations.

## Table of Contents

- [Class Diagram](#class-diagram)
- [Program Flow Diagrams](#program-flow-diagrams)
  - [Main Program Flow](#main-program-flow)
  - [Mode System Flow](#mode-system-flow)
  - [Service Integration Flow](#service-integration-flow)
  - [Button Interaction Flow](#button-interaction-flow)
  - [Mesh Communication Flow](#mesh-communication-flow)
  - [Available Modes Detail](#available-modes-detail)

## Class Diagram

This diagram shows the complete class structure and relationships within the GlowLight system.

```mermaid
classDiagram
    %% Main entry point
    class main {
        +Controller controller
        +LightService lightService
        +CommunicationService communicationService
        +DistanceService distanceService
        +GlowRegistry glowRegistry
        +setup()
        +loop()
    }

    %% Core Controller
    class Controller {
        -LightService* lightService
        -CommunicationService* communicationService
        -DistanceService* distanceService
        -GlowRegistry* glowRegistry
        -bool isButtonPressed
        -unsigned long lastButtonTime
        +setup(LightService*, CommunicationService*, DistanceService*, GlowRegistry*)
        +loop()
    }

    %% Services
    class LightService {
        -CRGB leds[NUM_LEDS]
        -AbstractMode* currentMode
        -uint8_t brightness
        -CRGB currentColor
        +setup()
        +setMode(AbstractMode*)
        +update()
        +setBrightness(uint8_t)
        +setColor(CRGB)
        +turnOff()
    }

    class CommunicationService {
        -bool connected
        -uint8_t peerList[MAX_PEERS][6]
        -int peerCount
        +setup()
        +loop()
        +sendAlert(Alert*)
        +scanForPeers()
        -setupESPNow()
        -addPeer()
    }

    class DistanceService {
        +setup()
        +getDistance()
    }

    class GlowRegistry {
        -AbstractMode* modes[MAX_MODES]
        -const char* modeNames[MAX_MODES]
        -int modeCount
        -int currentModeIndex
        -Preferences preferences
        +setup()
        +registerMode(const char*, AbstractMode*)
        +getMode(const char*)
        +getNextMode()
        +setCurrentMode(int)
        +getCurrentModeName()
    }

    %% Abstract Mode Interface
    class AbstractMode {
        <<abstract>>
        #unsigned long lastUpdate
        #unsigned long updateInterval
        +setup()* 
        +update(CRGB*, int)* 
        +reset()* 
    }

    %% Concrete Mode Implementations
    class StaticMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class RainbowMode {
        -uint8_t hue
        -uint8_t deltaHue
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class CandleMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class RandomGlowMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class SunsetMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class StrobeMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class BeaconMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    class ColorPickerMode {
        +setup()
        +update(CRGB*, int)
        +reset()
    }

    %% Helper Classes
    class Alert {
        -AlertType type
        -char message[256]
        -unsigned long timestamp
        +Alert(AlertType, const char*)
        +getType()
        +getMessage()
        +getTimestamp()
    }

    %% Relationships
    main --> Controller : creates
    main --> LightService : creates
    main --> CommunicationService : creates
    main --> DistanceService : creates
    main --> GlowRegistry : creates

    Controller --> LightService : uses
    Controller --> CommunicationService : uses
    Controller --> DistanceService : uses
    Controller --> GlowRegistry : uses

    LightService --> AbstractMode : uses current mode
    CommunicationService --> Alert : sends/receives
    GlowRegistry --> AbstractMode : manages all modes

    AbstractMode <|-- StaticMode : implements
    AbstractMode <|-- RainbowMode : implements
    AbstractMode <|-- CandleMode : implements
    AbstractMode <|-- RandomGlowMode : implements
    AbstractMode <|-- SunsetMode : implements
    AbstractMode <|-- StrobeMode : implements
    AbstractMode <|-- BeaconMode : implements
    AbstractMode <|-- ColorPickerMode : implements
```

## Program Flow Diagrams

### Main Program Flow

This diagram shows the complete program execution from system startup through the main loop.

```mermaid
flowchart TD
    A[System Start] --> B[setup Function]
    B --> C[Serial.begin 115200]
    C --> D[Wire.begin I2C Setup]
    D --> E[Services Setup]
    E --> E1[lightService.setup]
    E --> E2[distanceService.setup]
    E --> E3[communicationService.setup]
    E1 --> F[Button Setup]
    E2 --> F
    E3 --> F
    F --> F1[button.begin BUTTON_PIN]
    F1 --> F2[button.setLongClickTime 500]
    F2 --> G[Mode Registration]
    G --> G1[controller.addMode staticMode]
    G1 --> G2[controller.addMode colorPickerMode]
    G2 --> G3[controller.addMode rainbowMode]
    G3 --> G4[controller.addMode randomGlowMode]
    G4 --> H[Alert Mode Setup]
    H --> H1[controller.setAlertMode alertMode]
    H1 --> I[controller.setup]
    I --> J[Button Handler Setup]
    J --> J1[setLongClickHandler nextMode]
    J1 --> J2[setClickHandler nextOption]
    J2 --> J3[setDoubleClickHandler customClick]
    J3 --> K[Setup Complete]
    K --> L[loop Function Infinite Loop]
    L --> M[Service Loop Calls]
    M --> M1[button.loop]
    M --> M2[controller.loop]
    M --> M3[lightService.loop]
    M --> M4[distanceService.loop]
    M --> M5[communicationService.loop]
    M1 --> N{Button Event?}
    M2 --> O{Distance Change?}
    M3 --> P{LED Update?}
    M4 --> Q{Mesh Message?}
    M5 --> R[Update LEDs]
    N -->|Long Click| N1[controller.nextMode]
    N -->|Single Click| N2[controller.nextOption]
    N -->|Double Click| N3[controller.customClick]
    N -->|No Event| R
    O -->|Yes| O1[Process Distance Data]
    O -->|No| R
    O1 --> O2{ColorPicker Mode?}
    O2 -->|Yes| O3[Update Color Based on Distance]
    O2 -->|No| R
    P -->|Yes| P1[FastLED.show]
    P -->|No| R
    Q -->|Yes| Q1[Process Mesh Data]
    Q -->|No| R
    Q1 --> Q2[Synchronize with Other Lamps]
    N1 --> R
    N2 --> R
    N3 --> R
    O3 --> R
    P1 --> R
    Q2 --> R
    R --> L
```

### Mode System Flow

This diagram illustrates how lighting modes are managed, switched, and executed.

```mermaid
flowchart TD
    A[Mode System Start] --> B[Controller Setup]
    B --> C[Mode Array Initialization]
    C --> D[Current Mode = staticMode]
    D --> E{Button Long Click?}
    E -->|Yes| F[controller.nextMode]
    E -->|No| G{Button Single Click?}
    F --> F1[currentModeIndex++]
    F1 --> F2{Index >= modeCount?}
    F2 -->|Yes| F3[currentModeIndex = 0]
    F2 -->|No| F4[Switch to New Mode]
    F3 --> F4
    F4 --> F5[oldMode.last]
    F5 --> F6[newMode.setup]
    F6 --> F7[newMode.customFirst]
    F7 --> H[Mode Active]
    G -->|Yes| I[controller.nextOption]
    G -->|No| J{Button Double Click?}
    I --> I1[currentMode.nextOption]
    I1 --> H
    J -->|Yes| K[controller.customClick]
    J -->|No| H
    K --> K1[currentMode.customClick]
    K1 --> H
    H --> L[Mode Loop Execution]
    L --> L1[currentMode.customLoop]
    L1 --> L2[Update LEDs]
    L2 --> M{Distance Sensor Active?}
    M -->|Yes| N[Process Distance Input]
    M -->|No| O{Mesh Message?}
    N --> N1{ColorPicker Mode?}
    N1 -->|Yes| N2[Map Distance to Color]
    N1 -->|No| O
    N2 --> O
    O -->|Yes| P[Synchronize Mode State]
    O -->|No| Q[Continue Loop]
    P --> Q
    Q --> E
```

### Service Integration Flow

This diagram shows how the three core services interact with each other and the controller.

```mermaid
flowchart TD
    A[Service Integration] --> B[LightService]
    A --> C[DistanceService]
    A --> D[CommunicationService]
    B --> B1[FastLED Setup]
    B1 --> B2[LED Array Initialization]
    B2 --> B3[Default Brightness Setup]
    B3 --> B4{Mode Update Required?}
    B4 -->|Yes| B5[Update LED Colors]
    B4 -->|No| B6[FastLED.show]
    B5 --> B6
    B6 --> B7[Wait for Next Update]
    B7 --> B4
    C --> C1[VL53L0X Sensor Setup]
    C1 --> C2[I2C Communication Setup]
    C2 --> C3{Distance Reading Request?}
    C3 -->|Yes| C4[Read Sensor Data]
    C3 -->|No| C5[Wait]
    C4 --> C6[Process Distance Value]
    C6 --> C7{Valid Reading?}
    C7 -->|Yes| C8[Return Distance]
    C7 -->|No| C9[Return Error Value]
    C8 --> C5
    C9 --> C5
    C5 --> C3
    D --> D1[ESP-NOW Setup]
    D1 --> D2[Mesh Network Join]
    D2 --> D3[Peer Discovery]
    D3 --> D4{Mesh Message Received?}
    D4 -->|Yes| D5[Parse Message]
    D4 -->|No| D6{Send Message Required?}
    D5 --> D7{Mode Sync Message?}
    D7 -->|Yes| D8[Update Local Mode]
    D7 -->|No| D9[Process Other Message]
    D8 --> D6
    D9 --> D6
    D6 -->|Yes| D10[Broadcast to Mesh]
    D6 -->|No| D11[Continue]
    D10 --> D11
    D11 --> D4
    B8[LED Update Event] --> E[Controller Integration]
    C10[Distance Change Event] --> E
    D12[Mesh Sync Event] --> E
    E --> F{Event Type?}
    F -->|LED Update| G[Update Current Mode Display]
    F -->|Distance Change| H[Process Distance for Current Mode]
    F -->|Mesh Sync| I[Synchronize Mode State]
    G --> J[Services Continue]
    H --> J
    I --> J
    J --> B4
    J --> C3
    J --> D4
```

### Button Interaction Flow

This diagram details the button press handling and resulting actions.

```mermaid
flowchart TD
    A[Button Press Detected] --> B{Button Type?}
    B -->|Long Click 500ms+| C[nextMode]
    B -->|Single Click| D[nextOption]
    B -->|Double Click| E[customClick]
    C --> C1[Increment Mode Index]
    C1 --> C2{Index Overflow?}
    C2 -->|Yes| C3[Reset to First Mode]
    C2 -->|No| C4[Select New Mode]
    C3 --> C4
    C4 --> C5[Call currentMode.last]
    C5 --> C6[Call newMode.setup]
    C6 --> C7[Call newMode.customFirst]
    C7 --> F[Mode Switched]
    D --> D1[Call currentMode.nextOption]
    D1 --> D2[Mode-Specific Option Change]
    D2 --> G[Option Changed]
    E --> E1[Call currentMode.customClick]
    E1 --> E2[Mode-Specific Custom Action]
    E2 --> H[Custom Action Executed]
    F --> I[Continue Loop]
    G --> I
    H --> I
    I --> A
```

### Mesh Communication Flow

This diagram shows the ESP-NOW mesh networking and synchronization process.

```mermaid
flowchart TD
    A[Mesh Communication Start] --> B[ESP-NOW Initialization]
    B --> C[Peer Discovery]
    C --> D[Network Formation]
    D --> E{Message Type?}
    E -->|Incoming| F[Receive Message]
    E -->|Outgoing| G[Send Message]
    E -->|None| H[Idle State]
    F --> F1[Parse Message Header]
    F1 --> F2{Message Type?}
    F2 -->|Mode Sync| F3[Synchronize Mode]
    F2 -->|Alert| F4[Process Alert]
    F2 -->|Status| F5[Update Status]
    F3 --> I[Apply Changes]
    F4 --> I
    F5 --> I
    G --> G1[Prepare Message]
    G1 --> G2[Add Timestamp]
    G2 --> G3[Broadcast to Peers]
    G3 --> J[Message Sent]
    H --> K[Monitor Network]
    K --> L{Network Change?}
    L -->|Yes| C
    L -->|No| E
    I --> M[Update Local State]
    J --> N[Wait for ACK]
    M --> E
    N --> O{ACK Received?}
    O -->|Yes| E
    O -->|No| P[Retry Send]
    P --> G1
```

### Available Modes Detail

This diagram provides an overview of all available lighting modes and their features.

```mermaid
flowchart TD
    A[Available Modes] --> B[StaticMode]
    A --> C[ColorPickerMode] 
    A --> D[RainbowMode]
    A --> E[RandomGlowMode]
    A --> F[Alert Mode]
    
    G[Commented Modes] --> H[BeaconMode]
    G --> I[CandleMode]
    G --> J[SunsetMode]
    G --> K[StrobeMode]
    G --> L[MiniGame]
    
    B --> B1[Single Solid Color]
    B --> B2[Brightness Control]
    B --> B3[Color Selection via Distance]
    
    C --> C1[Distance-Based Color Selection]
    C --> C2[Real-time Color Change]
    C --> C3[Smooth Color Transitions]
    
    D --> D1[Flowing Rainbow Effect]
    D --> D2[Adjustable Speed]
    D --> D3[Mesh Synchronization]
    
    E --> E1[10 Scientific Colors]
    E --> E2[Speed Modes Zen to Party]
    E --> E3[Pause and Transition Cycles]
    
    F --> F1[System Notifications]
    F --> F2[Error Indicators]
    F --> F3[Status Communication]
