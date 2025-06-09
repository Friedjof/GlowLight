# DistanceService

Service für die Verwaltung des VL53L0X Laser-Abstandssensors mit Filterung und Kalibrierung.

## Funktionalität

Der DistanceService stellt präzise Abstandsmessungen und daraus abgeleitete Level-Werte bereit:

- **Laser-Abstandsmessung**: Hochpräzise Distanzmessung bis 2 Meter
- **Datenfilterung**: Glättung von Messwerten für stabile Ergebnisse
- **Level-Konvertierung**: Umwandlung von Abstand in normalisierte Level (0-100)
- **Kalibrierung**: Anpassbare Min/Max-Bereiche für verschiedene Anwendungen
- **Status-Monitoring**: Fehlerbehandlung und Sensorstatus-Überwachung

## Hardware-Integration

```
ESP32-C3 ──I2C──→ VL53L0X Sensor
│                   ┌─────────┐
├─ GPIO 6 (SDA) ──→ │   VCC   │ ←── 5V
├─ GPIO 7 (SCL) ──→ │   GND   │ ←── GND
└─ GND/5V       ──→ │ SDA SCL │ ←── I2C
                    └─────────┘
```

## Messbereich und Genauigkeit

```
Sensorbereich:    0mm ────────────→ 2000mm
                   │               │
Typische Nutzung: 10mm ────→ 200mm
                   │           │
Level-Mapping:    100% ────→  0%
```

## API-Übersicht

### Kern-Funktionen
- `setup()`: I2C- und Sensor-Initialisierung
- `loop()`: Kontinuierliche Messwert-Akquisition
- `getDistance()`: Aktuelle Entfernung in Millimetern
- `getLevel()`: Normalisierter Level-Wert (0-100)

### Datenverarbeitung
- `filter()`: Glättungsfilter für stabile Werte
- `distance2level()`: Konvertierung Abstand zu Level
- `getResult()`: Vollständiges Messergebnis mit Status

## Filterung

```
Rohdaten:     50, 48, 52, 45, 51, 49, 47, 53
Filter:       49, 49, 50, 49, 50, 50, 49, 50
              └─ Geglättete, stabile Werte
```

## Level-Berechnung

```cpp
// Beispiel: Hand-Nähe zu Helligkeits-Level
Abstand:  200mm  150mm  100mm   50mm   20mm
Level:      0%    25%    50%    75%   100%
Nutzung:   Aus   Dimm   Mittel  Hell   Max
```

## Konfiguration

Parameter in `GlowConfig.h`:
- `DISTANCE_MAX_MM`: Maximaler Messbereich
- `DISTANCE_MIN_MM`: Minimaler Messbereich  
- `DISTANCE_SDA_PIN`: I2C SDA-Pin
- `DISTANCE_SCL_PIN`: I2C SCL-Pin

## Status-Codes

- `0`: Erfolgreiche Messung
- `1`: Signal-Fehler (zu schwach)
- `2`: Phase-Fehler
- `4`: Hardware-Timeout
- `5`: Außerhalb des Bereichs

## Verwendung

```cpp
DistanceService distanceService;

// Initialisierung
distanceService.setup();

// In der Hauptschleife
distanceService.loop();

// Werte abrufen
uint16_t distance = distanceService.getDistance();
uint16_t level = distanceService.getLevel();
result_t result = distanceService.getResult();
```

## Anwendungen

- **Gestensteuerung**: Berührungsfreie Bedienung
- **Helligkeitssteuerung**: Automatische Lichtanpassung
- **Proximity-Erkennung**: Anwesenheitssensor
- **Interaktive Modi**: Spielsteuerung und Farbauswahl
