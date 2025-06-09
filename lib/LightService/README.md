# LightService

Zentrale Service-Klasse für die Steuerung der WS2812B LED-Strips mit sanften Übergängen.

## Funktionalität

Der LightService verwaltet alle LED-bezogenen Operationen und stellt eine einheitliche Schnittstelle bereit:

- **LED-Verwaltung**: Direkte Kontrolle über WS2812B LEDs
- **Sanfte Übergänge**: Automatische Interpolation zwischen Farbzuständen
- **Helligkeitssteuerung**: Globale Helligkeitsanpassung
- **Farboperationen**: Vielfältige Methoden zum Setzen von Farben
- **Performance-Optimierung**: Effiziente Updates nur bei Änderungen

## Hardware-Integration

```
ESP32-C3 ──→ GPIO 3 ──→ WS2812B LED Strip (11 LEDs)
                        ┌─── LED 0
                        ├─── LED 1
                        ├─── LED 2
                        ├─── ...
                        └─── LED 10
```

## API-Übersicht

### Grundfunktionen
- `setup()`: Initialisierung der LED-Hardware
- `loop()`: Automatische Übergangsaktualisierung
- `show()`: Sofortige LED-Aktualisierung

### Farbsteuerung
- `fill(r, g, b)`: Alle LEDs in RGB-Farbe
- `setLED(index, color)`: Einzelne LED setzen
- `setRange(start, end, color)`: LED-Bereich färben
- `clear()`: Alle LEDs ausschalten

### Effekte
- `fade()`: Sanftes Ein-/Ausblenden
- `setBrightness()`: Globale Helligkeit
- `setLightUpdateSteps()`: Übergangsgeschwindigkeit

## Sanfte Übergänge

```
Aktueller Zustand:  🔴 🔴 🔴 🔴 🔴
Ziel-Zustand:       🔵 🔵 🔵 🔵 🔵

Übergang Frame 1:   🟣 🟣 🟣 🟣 🟣  (25% zu Blau)
Übergang Frame 2:   🟪 🟪 🟪 🟪 🟪  (50% zu Blau)  
Übergang Frame 3:   💙 💙 💙 💙 💙  (75% zu Blau)
Übergang Frame 4:   🔵 🔵 🔵 🔵 🔵  (100% Blau)
```

## Konfiguration

Einstellbare Parameter in `GlowConfig.h`:
- `LED_NUM_LEDS`: Anzahl der LEDs (Standard: 11)
- `LED_PIN`: GPIO-Pin für LED-Strip (Standard: 3)
- `LED_DEFAULT_BRIGHTNESS`: Standard-Helligkeit
- `LED_UPDATE_STEPS`: Übergangsschritte für Sanftheit

## Verwendung

```cpp
LightService lightService;

// Initialisierung
lightService.setup();

// Alle LEDs rot
lightService.fill(255, 0, 0);

// Helligkeit auf 50%
lightService.setBrightness(128);

// Einzelne LED blau
lightService.setLED(5, CRGB::Blue);

// Updates in loop()
lightService.loop();
```

## Performance

- **Automatische Optimierung**: Updates nur bei Änderungen
- **Interpolation**: CPU-effiziente Farbübergänge
- **FastLED-Integration**: Optimierte Hardware-Kommunikation
- **Memory-Management**: Minimaler RAM-Verbrauch
