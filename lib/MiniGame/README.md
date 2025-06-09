# MiniGame

Ein interaktives Geschicklichkeitsspiel mit dem LED-Ring und Abstandssensor.

## Funktionalität

Das MiniGame ist ein reaktionsbasiertes Spiel, bei dem der Spieler einen bewegenden Lichtpunkt zum richtigen Zeitpunkt stoppen muss:

- **Bewegender Lichtpunkt**: Ein heller Punkt rotiert um den LED-Ring
- **Zielerkennung**: Spieler muss den Punkt in einer bestimmten Zone stoppen
- **Geschwindigkeitssteuerung**: Anpassbare Spielgeschwindigkeit
- **Gewinn-Animation**: Visuelle Belohnung bei erfolgreichem Treffer

## Visueller Effekt

```
    LED Ring Spiel
         🎯 ← Zielbereich
      ○     ○     
   ○           ○
○      🔵      ○  ← Bewegender Punkt
   ○           ○
      ○     ○
         ○
```

Spielablauf:
```
Start:    ● ○ ○ ○ ○ 🎯 ○ ○ ○ ○ ○  (Punkt startet, Ziel in Mitte)
Spiel:    ○ ○ ● ○ ○ 🎯 ○ ○ ○ ○ ○  (Punkt bewegt sich)
Treffer:  ○ ○ ○ ○ ○ 🟢 ○ ○ ○ ○ ○  (Erfolgreich gestoppt!)
```

## Spielregeln

1. **Start**: Lichtpunkt beginnt zu rotieren
2. **Zielen**: Warten bis der Punkt sich dem Ziel nähert
3. **Stoppen**: Doppelklick zum Stoppen des Punkts
4. **Gewinn**: Wenn der Punkt im Zielbereich gestoppt wird
5. **Neues Spiel**: Automatischer Neustart nach Gewinn/Verlust

## Spielmechanik

```
Bewegung:  ● → → → → 🎯 → → → → → ○
Timing:    ✗ ✗ ✗ ✓ ✓ ✓ ✓ ✗ ✗ ✗ ✗
           Zu früh    Perfekt    Zu spät
```

## Konfigurierbare Optionen

1. **Neue Geschwindigkeit**: Anpassung der Punktbewegung
   - Langsam: Einfacher für Anfänger
   - Schnell: Herausforderung für Experte

## Verwendung

Ideal für:
- **Unterhaltung**: Spaßiges Reaktionsspiel
- **Geschicklichkeitstraining**: Verbesserung der Reaktionszeit
- **Demonstrationen**: Zeigen der interaktiven Fähigkeiten
- **Kinderunterhaltung**: Einfaches, visuelles Spiel
- **Party-Gadget**: Multiplayer-Herausforderungen
- **Pausen**: Kurze Spielrunden zwischendurch

## Steuerung

- **Doppelklick**: Stoppt den bewegenden Punkt
- **Gestensensor**: Kann für alternative Steuerung genutzt werden
- **Automatischer Neustart**: Nach jeder Runde
