# BeaconMode

Ein rotierender Lichtpunkt-Modus, der wie ein Leuchtturm-Beacon funktioniert.

## Funktionalität

Der BeaconMode erstellt einen einzelnen hellen Lichtpunkt, der kontinuierlich um den LED-Ring rotiert. Dabei können verschiedene Parameter angepasst werden:

- **Geschwindigkeit**: Rotationsgeschwindigkeit des Lichtpunkts
- **Farbübergänge**: Zwei verschiedene Farbtöne für Variation
- **Weiche Übergänge**: Sanfte Bewegung zwischen den LEDs

## Visueller Effekt

```
    LED Ring (11 LEDs)
         ●
      ●     ●     ← Beacon startet hier
   ●           ●
●                 ●
   ●           ●
      ●     ●
         ○     ← und rotiert im Uhrzeigersinn
```

Animation:
```
Frame 1:  ● ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  (Beacon bei LED 0)
Frame 2:  ○ ● ○ ○ ○ ○ ○ ○ ○ ○ ○  (Beacon bei LED 1)
Frame 3:  ○ ○ ● ○ ○ ○ ○ ○ ○ ○ ○  (Beacon bei LED 2)
...       ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ●  (Beacon bei LED 10)
```

## Konfigurierbare Optionen

1. **Neue Geschwindigkeit**: Anpassung der Rotationsgeschwindigkeit
2. **Erster Farbton**: Primäre Beacon-Farbe
3. **Zweiter Farbton**: Sekundäre Beacon-Farbe für Farbwechsel

## Verwendung

Ideal für:
- Beruhigende, meditative Lichteffekte
- Nachtlicht mit sanfter Bewegung
- Signalisierung oder Aufmerksamkeit
- Orientierungslicht im Dunkeln
