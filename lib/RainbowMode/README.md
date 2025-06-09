# RainbowMode

Ein dynamischer Regenbogen-Modus mit sanften Farbübergängen über den gesamten LED-Ring.

## Funktionalität

Der RainbowMode erzeugt einen kontinuierlichen Regenbogeneffekt, bei dem alle Farben des Spektrums über die LEDs wandern:

- **Kontinuierlicher Farbkreislauf**: Alle Farben von Rot über Grün bis Blau
- **Fließende Übergänge**: Sanfte Farbverläufe zwischen benachbarten LEDs
- **Anpassbare Geschwindigkeit**: Steuerung der Farbwechsel-Geschwindigkeit
- **Sättigungssteuerung**: Intensität der Farben einstellbar

## Visueller Effekt

```
    LED Ring (Regenbogen-Verteilung)
         🔴
      🟠     🟡     
   🟢           🔵
🟣                 🟪
   💙           🤍
      ❤️     🧡
         💛
```

Farbanimation über Zeit:
```
Zeit 0:  🔴 🟠 🟡 🟢 🔵 🟣 🟪 💙 🤍 ❤️ 🧡
Zeit 1:  🧡 🔴 🟠 🟡 🟢 🔵 🟣 🟪 💙 🤍 ❤️
Zeit 2:  ❤️ 🧡 🔴 🟠 🟡 🟢 🔵 🟣 🟪 💙 🤍
Zeit 3:  🤍 ❤️ 🧡 🔴 🟠 🟡 🟢 🔵 🟣 🟪 💙
...
```

## Konfigurierbare Optionen

1. **Neue Sättigung**: Anpassung der Farbintensität (von pastellig bis leuchtend)
2. **Neue Geschwindigkeit**: Steuerung wie schnell die Farben rotieren

## Verwendung

Perfekt für:
- Entspannende Atmosphäre
- Farbtherapie und Stimmungsbeleuchtung
- Demonstration des vollen LED-Farbspektrums
- Lebendige Raumbeleuchtung
- Kinder-Nachtlicht mit beruhigenden Farben

## HSV-Farbmodell

Der Modus nutzt das HSV-Farbmodell (Hue, Saturation, Value) für:
- Gleichmäßige Farbverteilung
- Einfache Geschwindigkeitssteuerung
- Präzise Sättigungskontrolle
