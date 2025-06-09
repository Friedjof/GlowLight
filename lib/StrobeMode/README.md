# StrobeMode

High-energy synchronized strobe lighting system designed for party atmospheres with mesh network coordination and gesture controls.

## Features

- **⚡ Synchronized Strobing**: Perfect synchronization across all lamps using mesh time
- **🎵 Multiple Speed Settings**: 120, 180, 240, or 360 BPM strobe rates
- **🌈 Pattern Variety**: White, color cycle, random colors, and party palette
- **🚨 Emergency Stop**: Instant stop for epilepsy safety
- **👋 Gesture Controls**: Distance sensor for local effects and triggers
- **🌐 Mesh Coordination**: Automatic color distribution and wave effects

## Strobe Patterns

1. **White Strobe**: Classic pure white club effect
2. **Color Cycle**: Smooth rainbow color transitions with wave effects
3. **Random Colors**: Pseudo-random colors synchronized across lamps
4. **Party Palette**: Coordinated rotation through hot party colors

## Button Controls

- **Single Click**: Cycle through speed settings (120/180/240/360 BPM)
- **Double Click**: Emergency stop - immediately disables all strobing
- **Long Click**: Switch to next mode (standard behavior)

## Distance Sensor Effects (Local)

### Intensity Control
- **Very close (0-30mm)**: 150% brightness boost
- **Close (30-60mm)**: 125% brightness boost  
- **Medium (60-100mm)**: Normal brightness
- **Far (100mm+)**: 75% brightness

### Speed Boost
- **Close presence**: 50% faster strobing while hand is near

### Gesture Triggers
- **Quick hand movement**: Activates 5-second burst mode (3x speed)
- **Long presence (3+ seconds)**: Activates 10-second solo mode

## Mesh Network Synchronization

### Shared Time Synchronization
- Uses painlessMesh's `getNodeTime()` for perfect sync
- No master-slave architecture needed
- All lamps independently calculate identical strobe timing

### Global Messages
- **Speed changes**: Broadcasted to all lamps instantly
- **Emergency stop**: Immediately stops all strobing network-wide
- **Pattern changes**: Coordinated pattern switching

### Multi-Lamp Effects
- **Wave Mode**: Color cycle pattern creates wave effects across room
- **Color Coordination**: Each lamp gets assigned color based on Node ID
- **Synchronized Flashing**: Perfect room-wide strobe synchronization

## Safety Features

- **Epilepsy Warning**: Serial output warns about strobe activation
- **Emergency Stop**: Double-click instantly disables all strobing
- **Auto-timeout**: Could be extended to include automatic stop after time limit
- **Brightness Limiting**: Intensity controls prevent excessive brightness

## Technical Implementation

### Synchronization Formula
```
strobeActive = (meshTime + nodeOffset) % interval < flashDuration
```

### Mesh Time Advantages
- **No network latency**: All calculations local
- **Perfect sync**: Identical time reference across all lamps
- **Scalable**: Works with any number of lamps
- **Robust**: Survives temporary network interruptions

## Party Use Cases

- **Club Atmosphere**: Classic white strobe for dance floors
- **Color Parties**: Rainbow cycling with wave effects
- **Interactive Events**: Guests trigger effects via gestures
- **Coordinated Shows**: Multiple zones with different patterns
- **Emergency Safety**: Quick shutdown for epileptic emergencies

## Configuration

Settings automatically saved:
- Current speed setting
- Active pattern
- Emergency stop state

All settings persist between mode switches and device restarts.

## Warning

⚠️ **EPILEPSY WARNING**: This mode produces rapid flashing lights that may trigger seizures in individuals with photosensitive epilepsy. Always ensure emergency stop is accessible and inform guests about strobe lighting before activation.
