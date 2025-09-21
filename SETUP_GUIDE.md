# Orchestra M5GO Setup Guide

## Device Identification and Part Assignment

The Orchestra M5GO system supports multiple methods for identifying devices and assigning parts. Each of the 5 M5GO devices needs to know which part it should play in multi-part songs.

## Method 1: MAC Address Table (Recommended)

This is the most reliable method. Each M5GO has a unique MAC address that never changes.

### Step 1: Find Your M5GO MAC Addresses

1. Flash the firmware to each M5GO one at a time
2. Open the serial monitor (`pio device monitor`)
3. Look for the line: `Device MAC: XX:XX:XX:XX:XX:XX`
4. Write down each device's MAC address

### Step 2: Update the MAC Table

Edit `src/device_config.c` and update the `mac_table` array with your actual MAC addresses:

```c
static const device_info_t mac_table[] = {
    // Device 0 - Conductor/Part 1
    {{0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}, ROLE_CONDUCTOR, "M5GO-Conductor", false, 0},
    // Device 1 - Part 1 (First Violin)
    {{0x24, 0x6F, 0x28, 0x12, 0x34, 0x56}, ROLE_PART_1, "M5GO-Part1", false, 0},
    // Device 2 - Part 2 (Second Violin)
    {{0x24, 0x6F, 0x28, 0x78, 0x9A, 0xBC}, ROLE_PART_2, "M5GO-Part2", false, 0},
    // Device 3 - Part 3 (Viola)
    {{0x24, 0x6F, 0x28, 0xDE, 0xF0, 0x12}, ROLE_PART_3, "M5GO-Part3", false, 0},
    // Device 4 - Part 4 (Cello/Bass)
    {{0x24, 0x6F, 0x28, 0x34, 0x56, 0x78}, ROLE_PART_4, "M5GO-Part4", false, 0},
};
```

### Step 3: Set Configuration Method

In `src/orchestra.c`, ensure the initialization uses MAC table method:

```c
void orchestra_init(void) {
    // ...
    device_config_init(CONFIG_METHOD_MAC_TABLE);
    // ...
}
```

## Method 2: GPIO Hardware Configuration

Use physical jumpers or wires to set each device's ID.

### Wiring Setup

Connect the following GPIO pins to GND to set the device ID:

| Device ID | Role       | GPIO 34 | GPIO 35 | GPIO 36 |
|-----------|------------|---------|---------|---------|
| 0         | Conductor  | Open    | Open    | Open    |
| 1         | Part 1     | GND     | Open    | Open    |
| 2         | Part 2     | Open    | GND     | Open    |
| 3         | Part 3     | GND     | GND     | Open    |
| 4         | Part 4     | Open    | Open    | GND     |

### Configuration

In `src/orchestra.c`, set:
```c
device_config_init(CONFIG_METHOD_GPIO);
```

## Method 3: Auto-Assignment (Simplest)

Devices automatically get assigned roles based on discovery order.

### Setup

1. In `src/orchestra.c`, set:
```c
device_config_init(CONFIG_METHOD_AUTO_ASSIGN);
```

2. Power on devices in order:
   - First device becomes Conductor (Part 0)
   - Second device becomes Part 1
   - Third device becomes Part 2
   - And so on...

### Note
Auto-assignment is saved to NVS, so devices remember their roles after power cycle.

## Method 4: Pre-programmed (During Flashing)

Hardcode the device ID before flashing each device.

### Setup

1. Before flashing each device, edit `src/device_config.c`:
```c
static device_role_t current_role = ROLE_PART_1;  // Change for each device
```

2. Flash the device
3. Repeat for each M5GO with different role values

## ESP-NOW Discovery Process

### How Devices Find Each Other

1. **Broadcast Discovery**: All devices broadcast their presence using ESP-NOW broadcast address (FF:FF:FF:FF:FF:FF)

2. **Role Assignment**:
   - If a device has `ROLE_UNKNOWN`, it requests a role
   - The Conductor device assigns available roles

3. **Peer Registration**: Devices automatically register each other as ESP-NOW peers

4. **Synchronization**: Once all devices are discovered, they can play in sync

### Discovery Messages

The system uses these discovery messages:
- `ANNOUNCE`: "I'm here with role X"
- `ROLE_REQUEST`: "I need a role"
- `ROLE_ASSIGN`: "You are now role Y"
- `ROLL_CALL`: "Who's online?"
- `PRESENT`: "I'm online"
- `READY`: "I'm ready to play"

## Part Assignment in Songs

### Solo Songs
All devices play the same melody together.

### Duet Songs
- Parts 1 & 5 play together
- Then Parts 2 & 4 take over
- Conductor (Part 0) may coordinate

### Quintet Songs
Each device plays its own part:
- Part 0: Conductor/Lead melody
- Part 1: First harmony
- Part 2: Second harmony
- Part 3: Third harmony
- Part 4: Bass line

### Part Mask Explanation

Each song has a `parts_mask` that determines which devices play:
```c
#define PART_0   0x01  // Bit 0: Conductor
#define PART_1   0x02  // Bit 1: Part 1
#define PART_2   0x04  // Bit 2: Part 2
#define PART_3   0x08  // Bit 3: Part 3
#define PART_4   0x10  // Bit 4: Part 4
#define ALL_PARTS 0x1F // All 5 parts
```

Example for Canon in D (duet):
```c
.parts_mask = PART_1 | PART_5  // First section
// Later switches to:
.parts_mask = PART_2 | PART_4  // Second section
```

## Troubleshooting

### Devices Not Finding Each Other

1. **Check WiFi/ESP-NOW initialization**:
   - Look for "ESP-NOW initialized" in serial output
   - Ensure all devices are on same WiFi channel

2. **Verify MAC addresses**:
   - Broadcast MAC should be FF:FF:FF:FF:FF:FF
   - Check peer registration in logs

3. **Discovery timeout**:
   - Wait at least 5 seconds after power-on
   - Press Button A on any device to trigger re-discovery

### Wrong Part Assignment

1. **Check configuration method**:
   - Verify correct method in `device_config_init()`
   - For MAC table, ensure addresses are correct

2. **Clear NVS** (if using auto-assign):
   ```bash
   pio run --target erase_flash
   ```
   Then reflash firmware

3. **Verify GPIO connections** (if using hardware method):
   - Use multimeter to check GPIO pins
   - Ensure solid GND connections

### Synchronization Issues

1. **Check peer count**:
   - All 5 devices should be discovered
   - Look for "Added new peer" messages

2. **Verify timestamps**:
   - Messages include timestamps for sync
   - Large time differences indicate issues

3. **Signal strength**:
   - Keep devices within 10 meters
   - Avoid obstacles between devices

## Testing

### Basic Test Sequence

1. Power on all 5 M5GOs
2. Wait for blue LED (idle state)
3. Check serial output for successful initialization
4. Press Button A on any device
5. All devices should:
   - Change LED color (green for quintet)
   - Start playing synchronized music
   - Show animations on screen

### Individual Part Testing

To test if parts are correctly assigned:

1. Play Canon in D (Button B)
2. Only Parts 1&5 should play initially
3. Then Parts 2&4 take over
4. LED should be yellow (duet indicator)

### Discovery Test

1. Power on devices one at a time
2. Watch serial output for discovery messages
3. Each device should announce and get registered
4. Final count should be 4 peers (+ self = 5 total)