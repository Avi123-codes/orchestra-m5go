# Orchestra M5GO

ESP-IDF implementation of the Orchestra M5GO project - a synchronized multi-device music player using M5Stack Core devices.

## Features

- **Multi-device synchronization** via ESP-NOW protocol
- **7 songs** including solos, duets, and quintet pieces
- **RGB LED visualization** with color-coded song types:
  - Blue: Idle state
  - Green: Quintet pieces
  - Yellow: Duet pieces
  - Purple: Solo pieces
- **Screen animations** synchronized with music playback
- **Button controls**:
  - Button A: Cycles through Jupiter Hymn (quintet) and Carnival Theme (solo)
  - Button B: Cycles through Canon in D (duet), Carnival Variation, and Medallion Calls (solos)
  - Button C: Cycles through Blue Bells and TV Time (solos)

## Song Repertoire

1. **Jupiter Hymn** - Quintet piece from Holst's The Planets
2. **Canon in D** - Duet by Pachelbel
3. **Carnival of Venice Theme** - Solo piece
4. **Carnival of Venice Variation 1** - Solo piece
5. **Blue Bells of Scotland** - Traditional solo
6. **The Medallion Calls** - Solo from Pirates of the Caribbean
7. **It's TV Time!** - Solo from Deltarune

## Hardware Requirements

- 5x M5Stack Core ESP32 devices (M5GO)
- USB-C cables for programming
- Optional: Grove cables and connectors for physical connection

## Building and Flashing

### Prerequisites

- PlatformIO Core or PlatformIO IDE
- ESP-IDF framework (automatically managed by PlatformIO)

### Build Instructions

1. Clone the repository
2. Open the project in PlatformIO
3. Build the project:
   ```bash
   pio run
   ```

### Flashing to Devices

Each M5GO device needs to be flashed with the same firmware. The device ID can be configured via:
- Hardware jumpers (GPIO pins)
- NVS storage
- Or modified in code before flashing each device

To flash a device:
```bash
pio run --target upload
```

To monitor serial output:
```bash
pio device monitor
```

## Device Configuration

### Setting Device IDs

Each device in the orchestra needs a unique ID (0-4). This can be set by:

1. **Hardware Method**: Connect specific GPIO pins to ground
2. **Software Method**: Modify `device_id` in `orchestra.c` before flashing
3. **NVS Method**: Store ID in non-volatile storage

### ESP-NOW Setup

The devices automatically form an ESP-NOW network on startup. No WiFi configuration is needed as ESP-NOW operates independently.

## Usage

1. Power on all 5 M5GO devices
2. Wait for initialization (blue LEDs indicate ready state)
3. Press any button (A, B, or C) on any device to start a song
4. All devices will synchronize and play their respective parts
5. LEDs and screen animations will indicate the current song type

## Code Structure

```
src/
├── main.c           # Main application entry point
├── orchestra.c      # Main orchestra logic and coordination
├── songs.c          # Song data and melodies
├── audio.c          # Audio playback using I2S
├── display.c        # Screen control and animations
├── rgb_led.c        # RGB LED control
└── espnow_comm.c    # ESP-NOW communication

include/
├── orchestra.h      # Main orchestra definitions
└── songs.h          # Song structures and note definitions
```

## Development Notes

- The audio system uses the ESP32's internal DAC via I2S
- Display uses SPI to communicate with the ILI9342C LCD controller
- RGB LEDs are SK6812 compatible, controlled via RMT peripheral
- ESP-NOW broadcasts are used for synchronization between devices

## Troubleshooting

- **No sound**: Check volume setting and speaker connection
- **LEDs not working**: Verify GPIO 15 is correctly connected to LED data line
- **Devices not syncing**: Ensure all devices are powered and ESP-NOW is initialized
- **Button not responding**: Check GPIO pullup configuration and debounce timing

## License

This project is based on the original UIFlow implementation and adapted for ESP-IDF.