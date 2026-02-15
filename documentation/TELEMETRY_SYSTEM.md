# Telemetry System: RPI → Pico2

## Overview
Real-time telemetry system sending attitude warnings and status from Raspberry Pi to Pico2 via USB serial.

## Features

### Status Ribbon on Pico2
- **Dark gray ribbon** at top of **ALL** screens (28 pixels high)
  - Visible on: Root menu, GO FLY, BLUETOOTH, GYRO OFFSET, RADAR
  - Updates every 100ms
- **Right side**: Connection status icons (WiFi, GPS, Bluetooth)
  - Green: Connected/Active
  - Amber: Disconnected/Inactive
- **Left side**: Warning indicators (only shown when active)
  - Red warning triangle + "BANK" text when bank angle exceeds limit
  - Red warning triangle + "PITCH" text when pitch exceeds ±20°

### Telemetry Data Sent (Every 3 Seconds)
```json
{
  "own": {
    "lat": 0.0,
    "lon": 0.0,
    "alt": 0.0,
    "pitch": 2.5,
    "roll": -3.1,
    "yaw": 0.0
  },
  "traffic": [],
  "status": {
    "wifi": false,
    "gps": true,
    "bluetooth": false
  },
  "warnings": {
    "bank": false,
    "pitch": false
  }
}
```

## Warning Logic (RPI Side)

### Bank Angle Warning
- **Low speed (≤85 knots)**: Triggers at ±20° bank
- **High speed (>85 knots)**: Triggers at ±30° bank
- Calculated from GPS groundspeed

### Pitch Angle Warning
- Triggers at ±20° pitch (nose up or down)
- Displayed on both RPI HUD and Pico2 ribbon

## Implementation Files

### Pico2 Changes
1. **telemetry_parser.h** - Added `WarningStatus` structure
2. **telemetry_parser.c** - Parse `warnings` JSON section
3. **img/warning_icon.h** - 24x24 warning triangle icon
4. **st7789_lcd.h/c** - Added `lcd_draw_warning_icon()` function
5. **main_menu.c** - Updated `draw_status_icons()` with ribbon and warnings
6. **menu.c** - Draw ribbon on menu screens, adjusted menu item positions to y=32
7. **main_menu.c** - Main loop reads telemetry and updates ribbon every 100ms

### RPI Changes
1. **main.c** - Added `send_telemetry_to_pico()` function
2. **main.c** - Telemetry sent every 3 seconds in main loop
3. **main.c** - WiFi status placeholder (currently hardcoded to false)

## Update Rates
- **Attitude sensor**: 15ms (~67 Hz) - High priority for smooth display
- **GPS updates**: 200ms (5 Hz)
- **Telemetry to Pico**: 3000ms (every 3 seconds) - Low frequency to avoid interfering with attitude indicator
- **Display updates**: On-change (immediate)

## Color Scheme
- **Ribbon background**: 0x2104 (dark gray)
- **Warning active**: COLOR_RED
- **Status connected**: COLOR_GREEN
- **Status disconnected**: COLOR_AMBER

## Usage

### Compile RPI
```bash
cd rpi/c
gcc -O2 -Wall -Wextra -o pilot_assistant src/main.c src/st7789_rpi.c src/adxl345.c src/gps.c -I./include -lgpiod -lm
```

### Compile Pico2
```bash
cd pico/c/build
export PICO_SDK_PATH=~/pico-sdk
make menu_system -j4
```

### Deploy Pico2
1. Hold BOOTSEL button, plug in USB
2. Copy firmware: `cp menu_system.uf2 /media/sebastien/RP2350/`
3. Pico2 automatically reboots

### Run
1. Start RPI: `./pilot_assistant`
2. Warning ribbon appears on all Pico2 menu screens
3. Warnings update automatically based on flight conditions

## Future Enhancements
- Actual WiFi status detection (check network interface)
- Add Bluetooth status detection
- Traffic data display on RADAR page
- Altitude warnings
- Speed warnings (Vne, stall speed)
