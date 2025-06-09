# GlowRegistry

Persistenter Einstellungs-Speicher für Konfigurationsdaten mit EEPROM-Integration.

## Funktionalität

Die GlowRegistry bietet ein typsicheres System zur Speicherung und Verwaltung von Einstellungen:

- **Persistente Speicherung**: Automatische EEPROM-Synchronisation
- **Typsicherheit**: Unterstützung für verschiedene Datentypen
- **JSON-basiert**: Strukturierte Datenhaltung
- **Metadaten**: Zusätzliche Informationen zu gespeicherten Werten
- **Hot-Reload**: Sofortige Verfügbarkeit nach Änderungen

## Unterstützte Datentypen

```cpp
enum RegistryType {
    INT = 0,      // Ganzzahlen (int16_t, uint16_t, etc.)
    STRING = 1,   // Text-Strings
    BOOL = 2,     // Boolean-Werte (true/false)
    COLOR = 3     // CRGB Farbwerte (als Hex gespeichert)
};
```

## Datenstruktur

```json
Registry:
{
    "mode_speed": 50,
    "favorite_color": "#FF5500",
    "auto_brightness": true,
    "device_name": "GlowLight_01"
}

Metadaten:
{
    "mode_speed": {"type": 0, "min": 1, "max": 100},
    "favorite_color": {"type": 3, "format": "hex"},
    "auto_brightness": {"type": 2, "default": true},
    "device_name": {"type": 1, "maxlen": 32}
}
```

## API-Übersicht

### Basis-Operationen
- `set(key, value)`: Wert speichern
- `get(key)`: Wert abrufen
- `has(key)`: Existenz prüfen
- `remove(key)`: Wert löschen

### Typ-spezifische Getter
- `getInt(key)`: Integer-Wert
- `getString(key)`: String-Wert
- `getBool(key)`: Boolean-Wert
- `getColor(key)`: CRGB-Farbwert

### Persistierung
- `save()`: In EEPROM speichern
- `load()`: Aus EEPROM laden
- `clear()`: Registry leeren
- `factory_reset()`: Auf Standardwerte zurücksetzen

## Farbkonvertierung

```cpp
// CRGB zu Hex-String
CRGB red = CRGB::Red;
String hex = CRGB2Hex(red);  // "#FF0000"

// Hex-String zu CRGB
String color = "#00FF00";
CRGB green = Hex2CRGB(color);  // CRGB::Green
```

## Verwendungsbeispiele

### Einstellungen speichern
```cpp
GlowRegistry registry;

// Verschiedene Datentypen
registry.set("brightness", 75);
registry.set("device_name", "Bedroom_Lamp");
registry.set("enabled", true);
registry.set("accent_color", CRGB::Blue);

// Dauerhaft speichern
registry.save();
```

### Einstellungen laden
```cpp
// Beim Systemstart
registry.load();

// Werte abrufen
int brightness = registry.getInt("brightness");
String name = registry.getString("device_name");
bool enabled = registry.getBool("enabled");
CRGB color = registry.getColor("accent_color");
```

### Standardwerte
```cpp
// Mit Fallback-Werten
int speed = registry.has("speed") ? 
            registry.getInt("speed") : 50;  // Standard: 50

// Oder direkt mit Default
String mode = registry.getString("mode", "static");  // Standard: "static"
```

## Modus-spezifische Einstellungen

```cpp
// RainbowMode Einstellungen
registry.set("rainbow_speed", 30);
registry.set("rainbow_saturation", 255);

// BeaconMode Einstellungen  
registry.set("beacon_color1", CRGB::Red);
registry.set("beacon_color2", CRGB::Blue);
registry.set("beacon_speed", 25);

// ColorPicker Favoriten
registry.set("favorite_colors", "[\"#FF0000\", \"#00FF00\", \"#0000FF\"]");
```

## Memory Management

- **Kompakte Speicherung**: JSON-Kompression für EEPROM
- **Lazy Loading**: Nur bei Bedarf aus EEPROM lesen
- **Dirty Tracking**: Nur geänderte Werte speichern
- **Wear Leveling**: EEPROM-Lebensdauer optimieren

## Integration

Jeder AbstractMode kann die Registry nutzen:
```cpp
class MyMode : public AbstractMode {
    void setup() {
        // Einstellungen laden
        speed = registry.getInt("my_mode_speed", 50);
        color = registry.getColor("my_mode_color", CRGB::White);
    }
    
    void saveSettings() {
        // Einstellungen speichern
        registry.set("my_mode_speed", speed);
        registry.set("my_mode_color", color);
        registry.save();
    }
};
```
