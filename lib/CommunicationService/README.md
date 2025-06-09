# CommunicationService

Mesh-Netzwerk Service für drahtlose Kommunikation zwischen mehreren GlowLight-Lampen.

## Funktionalität

Der CommunicationService ermöglicht die Vernetzung mehrerer Lampen zu einem synchronisierten Mesh-Netzwerk:

- **Mesh-Netzwerk**: Selbstorganisierendes drahtloses Netzwerk
- **Synchronisation**: Gleichzeitige Modi und Effekte auf allen Lampen
- **Node-Management**: Automatische Erkennung und Verwaltung verbundener Lampen
- **Message-System**: Verschiedene Nachrichtentypen für unterschiedliche Zwecke
- **Heartbeat-Monitoring**: Überwachung der Netzwerk-Gesundheit

## Netzwerk-Topologie

```
    GlowLight Mesh Network
    
    Lampe A ←─────→ Lampe B
       ↑               ↓
       │               │
       └──→ Lampe C ←──┘
            ↕️
         Lampe D
         
    Alle Lampen können miteinander kommunizieren
```

## Nachrichtentypen

### 1. EVENT - Ereignis-Synchronisation
```
Lampe 1: [Modus gewechselt] ──broadcast──→ Alle anderen
Resultat: Alle Lampen wechseln synchron den Modus
```

### 2. SYNC - Zustandsabgleich
```
Neue Lampe: [Sync-Request] ──→ Netzwerk
Antwort:    [Aktueller Zustand] ←── Andere Lampen
```

### 3. HEARTBEAT - Lebenszeichen
```
Jede Lampe: [Ich bin online] ──periodic──→ Netzwerk
Zweck: Erkennung von Verbindungsabbrüchen
```

### 4. WIPE - Netzwerk-Reset
```
Admin-Lampe: [Reset alle] ──broadcast──→ Netzwerk
Resultat: Alle Lampen kehren zu Standardzustand zurück
```

## Node-Management

```cpp
struct GlowNode {
    uint32_t id;           // Eindeutige Lampen-ID
    uint64_t lastSeen;     // Letzter Kontakt
};
```

Automatische Funktionen:
- **Discovery**: Neue Lampen werden automatisch erkannt
- **Timeout**: Offline-Lampen werden aus der Liste entfernt
- **Reconnection**: Wiederverbindung nach Unterbrechungen

## Mesh-Konfiguration

Parameter in `GlowConfig.h`:
- `MESH_SSID`: Netzwerk-Name (Standard: "GlowMesh")
- `MESH_PASSWORD`: Netzwerk-Passwort (Standard: "GlowMesh")
- `MESH_PORT`: Kommunikations-Port
- `MESH_CHANNEL`: WiFi-Kanal

## Sicherheit

- **Passwort-Schutz**: Verhindert unbefugten Zugriff
- **Netzwerk-Isolation**: Getrennte Gruppen durch verschiedene SSIDs
- **Verschlüsselung**: Sichere Datenübertragung

## Verwendung

```cpp
CommunicationService commService;

// Initialisierung
commService.setup();

// Event senden (z.B. Modus-Wechsel)
commService.sendEvent("mode_change", "rainbow");

// Sync-Request
commService.requestSync();

// Node-Liste abrufen
auto nodes = commService.getConnectedNodes();

// In der Hauptschleife
commService.loop();
```

## Synchronisation-Beispiele

### Modus-Synchronisation
```
Lampe 1: Benutzer wechselt zu Rainbow-Modus
      ↓
Lampe 1: Sendet EVENT("mode", "rainbow")
      ↓
Lampe 2,3,4: Empfangen Nachricht und wechseln auch zu Rainbow
```

### Farb-Synchronisation
```
Lampe 1: ColorPicker wählt Rot
      ↓
Lampe 1: Sendet EVENT("color", "#FF0000")
      ↓
Alle anderen: Wechseln zu derselben roten Farbe
```

## Technische Details

- **Framework**: PainlessMesh Library
- **Protokoll**: JSON-basierte Nachrichten
- **Auto-Healing**: Automatische Netzwerk-Reparatur
- **Skalierbar**: Unterstützt theoretisch unbegrenzt viele Knoten
