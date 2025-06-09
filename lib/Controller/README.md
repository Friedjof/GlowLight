# Controller

Zentrale Steuerungseinheit für das Management aller Beleuchtungsmodi und Services.

## Funktionalität

Der Controller ist das Herzstück des GlowLight-Systems und orchestriert alle Komponenten:

- **Modus-Management**: Verwaltung und Wechsel zwischen verschiedenen Beleuchtungsmodi
- **Service-Koordination**: Integration von LightService, DistanceService und CommunicationService
- **Alert-System**: Visuelle Benachrichtigungen und Statusanzeigen
- **Event-Handling**: Verarbeitung von Button-Klicks und Sensor-Events
- **State-Management**: Zustandserhaltung und Modus-Wiederherstellung

## System-Architektur

```
                    Controller
                       │
        ┌──────────────┼──────────────┐
        │              │              │
   LightService   DistanceService  CommService
        │              │              │
    ┌───▼───┐      ┌───▼───┐      ┌───▼───┐
    │ LEDs  │      │Sensor │      │ Mesh  │
    └───────┘      └───────┘      └───────┘
```

## Modus-Verwaltung

```cpp
Verfügbare Modi:
├── StaticMode      (Statische Farben)
├── RainbowMode     (Regenbogen-Effekt)
├── BeaconMode      (Rotierender Lichtpunkt)
├── CandleMode      (Kerzenlicht-Simulation)
├── ColorPickerMode (Interaktive Farbwahl)
└── MiniGame        (Reaktionsspiel)
```

## Navigation zwischen Modi

```
Button-Klick:     StaticMode → RainbowMode → BeaconMode → ...
                      ↑                                    │
                      └────────────────────────────────────┘
                           (Zurück zum Anfang)
```

## Alert-System

Visuelle Benachrichtigungen für verschiedene Systemereignisse:

```
Modus-Wechsel:    💚 💚 💚 (3x Grün)
Fehler:           🔴 🔴 🔴 (3x Rot)
Warnung:          🟡 🟡 🟡 (3x Gelb)
Verbindung:       🔵 🔵 🔵 (3x Blau)
```

## Event-Verarbeitung

### Button-Events
- **Einfach-Klick**: Nächster Modus
- **Doppel-Klick**: Modus-spezifische Aktion
- **Langer Druck**: System-Optionen (optional)

### Sensor-Events
- **Proximity-Änderung**: Helligkeit anpassen
- **Gesten**: Modus-spezifische Interaktionen

### Mesh-Events
- **Neue Node**: Synchronisation initiieren
- **Mode-Sync**: Modi auf anderen Lampen synchronisieren
- **Disconnect**: Offline-Lampen aus Liste entfernen

## Zustandsmanagement

```cpp
Controller-Zustand:
├── currentModeIndex    // Aktiver Modus (0-N)
├── currentMode        // Pointer auf aktiven Modus
├── previousMode       // Letzter Modus (für Rollback)
├── alertMode          // Alert-Status
└── services[]         // Referenzen zu allen Services
```

## API-Übersicht

### Modus-Steuerung
- `addMode()`: Neuen Modus hinzufügen
- `nextMode()`: Zum nächsten Modus wechseln
- `setMode()`: Direkter Modus-Wechsel
- `getCurrentMode()`: Aktuellen Modus abrufen

### System-Steuerung
- `setup()`: Controller-Initialisierung
- `loop()`: Hauptverarbeitungsschleife
- `handleClick()`: Button-Event verarbeiten
- `enableAlert()`: Visuelle Benachrichtigung

## Verwendung

```cpp
Controller controller;

// Services hinzufügen
controller.addService(&lightService);
controller.addService(&distanceService);
controller.addService(&commService);

// Modi registrieren
controller.addMode(new StaticMode(...));
controller.addMode(new RainbowMode(...));

// Initialisierung
controller.setup();

// Hauptschleife
void loop() {
    controller.loop();
}
```

## Synchronisation

Bei Mesh-Netzwerken synchronisiert der Controller:
- **Modus-Wechsel**: Alle Lampen wechseln gemeinsam
- **Konfigurationen**: Farben, Geschwindigkeiten etc.
- **Alerts**: System-Benachrichtigungen im Netzwerk
