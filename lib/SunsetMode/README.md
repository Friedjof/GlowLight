# SunsetMode

A natural sunset simulation mode designed to support circadian rhythms and provide a peaceful bedtime routine.

## Features

- **🌅 Natural Color Progression**: Four distinct phases mimicking real sunset colors
- **⏱️ Multiple Durations**: 5, 15, 30, or 60-minute sunset options
- **😴 Sleep Mode**: Automatically stays off when sunset completes
- **🔘 Manual Shutdown**: Double-click to force immediate shutdown
- **🌐 Mesh Synchronization**: Synchronized sunset across multiple lamps
- **👋 Gesture Control**: Distance sensor for duration adjustment

## Sunset Phases

1. **Golden Hour (0-25%)**: Warm white → Golden yellow
2. **Orange Glow (25-50%)**: Golden yellow → Deep orange  
3. **Red Horizon (50-85%)**: Deep orange → Deep red
4. **Twilight Fade (85-100%)**: Deep red → Off

## Button Controls

- **Single Click**: Cycle through duration options (5/15/30/60 minutes)
- **Double Click**: Manual shutdown - lamp stays off until mode change
- **Long Click**: Switch to next mode (standard behavior)

## Duration Feedback

When mode starts, the lamp briefly flashes blue to indicate current duration:
- 1 flash = 5 minutes
- 2 flashes = 15 minutes  
- 3 flashes = 30 minutes
- 4 flashes = 60 minutes

## Gesture Control

Use hand proximity over the distance sensor to adjust sunset duration while in configuration mode.

## Mesh Network Integration

- **Synchronized Start**: All lamps begin sunset simultaneously
- **Manual Shutdown Broadcasting**: When one lamp is manually shut down, others follow
- **Duration Synchronization**: Master lamp coordinates sunset timing

## Use Cases

- **Bedtime Routine**: 15-30 minute sunset for natural sleep preparation
- **Reading**: 60-minute extended sunset for long reading sessions
- **Meditation**: Any duration for mindfulness and relaxation
- **Child Bedtime**: Shorter 5-minute sunset for quick routine

## Technical Details

- **Color Temperature**: Starts at 3000K, ends at complete darkness
- **Brightness Curve**: Logarithmic fade for natural perception
- **Memory**: Remembers duration setting and manual shutdown state
- **Network Messages**: JSON-based mesh communication for synchronization

## Configuration

The mode automatically saves:
- Selected duration setting
- Manual shutdown state
- Sunset active status

Settings persist between mode switches and device restarts.
