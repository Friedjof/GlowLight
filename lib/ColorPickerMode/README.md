# ColorPickerMode

Ein interaktiver Modus zur Farbauswahl mit gestenbasierter Steuerung über den Abstandssensor.

## Funktionalität

Der ColorPickerMode ermöglicht es, Farben durch Handbewegungen über dem Abstandssensor auszuwählen:

- **Gestenbasierte Farbwahl**: Handposition bestimmt Farbton (Hue)
- **Interaktive Sättigung**: Anpassung der Farbintensität
- **Echtzeit-Vorschau**: Sofortige Farbdarstellung während der Auswahl
- **HSV-Farbmodell**: Intuitive Farbauswahl über Farbkreis

## Visueller Effekt

```
    Farbkreis-Navigation

        🔴 (0°)
    🟪         🟠
🔵               🟡
    🟢   🖐️   🟢   ← Hand steuert Farbposition
🟦               🟡
    💙         ❤️
        🟣 (180°)
```

Interaktive Farbauswahl:
```
Hand nah (20mm):    🔴 🔴 🔴 🔴 🔴 🔴 🔴 🔴 🔴 🔴 🔴  (Rot)
Hand mittel (60mm): 🟡 🟡 🟡 🟡 🟡 🟡 🟡 🟡 🟡 🟡 🟡  (Gelb)
Hand weit (100mm):  🔵 🔵 🔵 🔵 🔵 🔵 🔵 🔵 🔵 🔵 🔵  (Blau)
```

## Steuerung

**Abstandsbasierte Farbauswahl:**
- **0-40mm**: Rottöne (0°-60° im Farbkreis)
- **40-80mm**: Gelb-Grüntöne (60°-180° im Farbkreis)  
- **80-120mm**: Blau-Violetttöne (180°-300° im Farbkreis)
- **120mm+**: Magenta-Rottöne (300°-360° im Farbkreis)

## Konfigurierbare Optionen

1. **Neuer Farbton**: Manuelle Farbton-Einstellung
2. **Neue Sättigung**: Anpassung der Farbintensität
   - Niedrig: Pastellfarben
   - Hoch: Leuchtende, gesättigte Farben

## Verwendung

Perfekt für:
- **Interaktive Farbauswahl**: Spielerische Bedienung
- **Stimmungsbeleuchtung**: Farbe passend zur Situation
- **Demonstration**: Zeigen der Sensor-Funktionalität
- **Personalisierung**: Individuelle Liebingsfarben einstellen
- **Accessibility**: Berührungsfreie Bedienung
- **Kinder**: Intuitive, magische Farbsteuerung

## Technische Details

- Nutzt `distance2hue()` Funktion zur Entfernungs-Farbton-Konvertierung
- HSV-Farbmodell für gleichmäßige Farbverteilung
- Echtzeit-Feedback über LED-Ring
