# Alert

Spezieller Modus für visuelle System-Benachrichtigungen und Statusanzeigen.

## Funktionalität

Die Alert-Klasse stellt ein visuelles Benachrichtigungssystem bereit:

- **Blitzende Signale**: Konfigurierbare Anzahl von Lichtblitzen
- **Farbcodierte Alerts**: Verschiedene Farben für unterschiedliche Nachrichten
- **Temporärer Modus**: Automatische Rückkehr zum vorherigen Zustand
- **Non-blocking**: Unterbricht andere Modi nur kurzzeitig
- **Systemintegration**: Wird vom Controller für Status-Updates genutzt

## Visuelle Signale

```
Normal:     ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  (LEDs aus)
Blitz 1:    ● ● ● ● ● ● ● ● ● ● ●  (Alle LEDs an)
Pause:      ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  (Kurze Pause)
Blitz 2:    ● ● ● ● ● ● ● ● ● ● ●  (Alle LEDs an)
...
Ende:       [Zurück zum vorherigen Modus]
```

## Farbkodierung

Standardmäßige Bedeutungen für Alert-Farben:

```
🔴 Rot:     Fehler, kritische Probleme
🟡 Gelb:    Warnungen, wichtige Hinweise  
🟢 Grün:    Erfolg, positive Bestätigung
🔵 Blau:    Information, Status-Updates
🟣 Lila:    System-Events, Konfiguration
🟠 Orange:  Standard-Alert (Warm Pink)
```

## Anwendungsfälle

### System-Events
```cpp
// Modus-Wechsel bestätigen
alert.setColor(CRGB::Green);
alert.setFlashes(2);

// Verbindungsfehler anzeigen  
alert.setColor(CRGB::Red);
alert.setFlashes(3);

// Neue Mesh-Node erkannt
alert.setColor(CRGB::Blue);
alert.setFlashes(1);
```

### Benutzer-Feedback
```cpp
// Einstellung gespeichert
alert.setColor(CRGB::Green);
alert.setFlashes(1);

// Ungültige Eingabe
alert.setColor(CRGB::Yellow);
alert.setFlashes(2);

// Factory Reset
alert.setColor(CRGB::Purple);
alert.setFlashes(5);
```

## Timing und Verhalten

```
Flash-Sequenz:
├── Flash 1: 200ms AN  → 100ms AUS
├── Flash 2: 200ms AN  → 100ms AUS  
├── Flash 3: 200ms AN  → 100ms AUS
└── Ende: Rückkehr zum vorherigen Modus
```

## API-Übersicht

### Konfiguration
- `setFlashes(count)`: Anzahl der Blitze (1-10)
- `setColor(color)`: Farbe des Alerts
- `getColor()`: Aktuelle Alert-Farbe

### Status
- `isFlashing()`: Prüfen ob Alert aktiv
- `customClick()`: Alert-spezifische Aktionen

### Lifecycle
- `setup()`: Initialisierung
- `customFirst()`: Start der Blitz-Sequenz
- `customLoop()`: Blitz-Animation verwalten
- `last()`: Aufräumen nach Alert

## Controller-Integration

```cpp
// Im Controller
void Controller::showAlert(AlertType type) {
    switch(type) {
        case SUCCESS:
            alert->setColor(CRGB::Green);
            alert->setFlashes(2);
            break;
        case ERROR:
            alert->setColor(CRGB::Red);  
            alert->setFlashes(3);
            break;
        case INFO:
            alert->setColor(CRGB::Blue);
            alert->setFlashes(1);
            break;
    }
    setMode(alert);  // Temporär zu Alert wechseln
}
```

## ESP-NOW-Synchronisation

Alerts sind lokale, kurzlebige Controller-Zustände und werden derzeit nicht als
Mode-Commands synchronisiert.

## Anpassung

```cpp
// Eigene Alert-Farben definieren
#define ALERT_SUCCESS    CRGB(0, 255, 0)    // Grün
#define ALERT_WARNING    CRGB(255, 255, 0)  // Gelb
#define ALERT_ERROR      CRGB(255, 0, 0)    // Rot
#define ALERT_INFO       CRGB(0, 0, 255)    // Blau
#define ALERT_CUSTOM     CRGB(255, 128, 20) // Warm Pink
```
