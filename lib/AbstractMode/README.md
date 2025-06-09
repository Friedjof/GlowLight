# AbstractMode

Abstrakte Basisklasse für alle Beleuchtungsmodi der GlowLight-Lampe.

## Funktionalität

Die `AbstractMode` Klasse stellt die grundlegende Struktur und gemeinsame Funktionalitäten für alle Beleuchtungsmodi bereit:

- **Optionen-System**: Verwaltung konfigurierbarer Optionen für jeden Modus
- **Helligkeitssteuerung**: Automatische Helligkeitsanpassung basierend auf Abstandssensor
- **Service-Integration**: Zugriff auf LightService, DistanceService und CommunicationService
- **Serialisierung**: JSON-basierte Konfigurationsspeicherung und -wiederherstellung
- **Normalisierung**: Mathematische Hilfsfunktionen für Wertebereiche

## Interface

Jeder Modus muss folgende virtuelle Methoden implementieren:

- `setup()`: Einmalige Initialisierung des Modus
- `customFirst()`: Wird beim ersten Aktivieren aufgerufen
- `customLoop()`: Hauptschleife des Modus
- `last()`: Aufräumarbeiten beim Verlassen des Modus
- `customClick()`: Behandlung von Doppelklick-Events

## Klassen-Hierarchie

```
AbstractMode (abstract)
├── BeaconMode
├── CandleMode
├── ColorPickerMode
├── MiniGame
├── RainbowMode
└── StaticMode
```

## Verwendung

```cpp
class MyMode : public AbstractMode {
public:
    MyMode(LightService* light, DistanceService* distance, CommunicationService* comm)
        : AbstractMode(light, distance, comm) {}
    
    void setup() override { /* Initialisierung */ }
    void customFirst() override { /* Erster Aufruf */ }
    void customLoop() override { /* Hauptlogik */ }
    void last() override { /* Aufräumen */ }
    void customClick() override { /* Doppelklick */ }
};
```
