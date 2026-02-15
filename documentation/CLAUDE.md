# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PilotAssistant is a dual-platform pilot assistant system:
- **Raspberry Pi** (rpi/): Full-featured system with 240x240 ST7789 LCD, GPS, camera, sensors
- **Pico2** (pico/): Lightweight C implementation for RP2350 with LCD and input handling

## Project Structure

```
/
├── rpi/              # Raspberry Pi Python implementation
│   ├── main.py       # Main entry point
│   ├── library/      # Core libraries (ST7789, config, state)
│   ├── menu/         # Menu modules (GPS, camera, gyro, etc.)
│   ├── service/      # Background services (GPS, attitude, traffic)
│   ├── debug/        # Debug/test scripts
│   └── images/       # Image assets
├── pico/             # Pico2 C/MicroPython implementations
│   ├── c/            # C implementation (telemetry, input handler)
│   └── microPython/  # MicroPython implementation
└── Simulator/        # Testing utilities (telemetry sender, monitor)
```

## Raspberry Pi Architecture (rpi/)

### Core Components

- **Main Entry Point**: `rpi/main.py` - Main menu system with 6 options (Gyro, AI-Camera, GPS, Traffic, Bluetooth, Go FLY)
- **Display System**: `rpi/library/ST7789.py` - ST7789 LCD display driver with 240x240 resolution
- **Hardware Configuration**: `rpi/library/config.py` - GPIO pin definitions, SPI setup, and hardware interfaces
- **State Management**: `rpi/library/pico_state.py` - Shared state for Pico2 button inputs
- **Menu Modules**: `rpi/menu/` directory contains specialized menu pages:
  - `gyro_menu.py` - Pitch and bank calibration interface
  - `gps_menu.py` - GPS coordinate display and NMEA parsing
  - `stream_menu.py` - Camera/streaming functionality
  - `traffic_menu.py` - Traffic display
  - `bluetooth_menu.py` - Bluetooth configuration
  - `fly_menu.py` - Flight display

### Hardware Interface (Raspberry Pi)

The system uses the following GPIO pins (defined in `rpi/library/config.py`):
- Joystick: UP(6), DOWN(19), LEFT(5), RIGHT(26), PRESS(13)
- Additional keys: KEY1(21), KEY2(20), KEY3(16)
- GPS communication via serial port
- MPU-6050 6-axis IMU (accelerometer + gyroscope) via I2C
- Camera via Pi Camera interface
- Pico2 communication via USB serial (/dev/ttyACM0)

### Display System

- 240x240 pixel ST7789 LCD with SPI communication
- Images rotated 270° for proper orientation
- Menu system with 6 main options positioned at specific coordinates
- Color scheme: CYAN for text/selection, BLACK backgrounds, MAGENTA for titles

## Pico2 Architecture (pico/)

### C Implementation (pico/c/)

- **Telemetry Receiver**: `main.c` - Receives JSON telemetry via USB and displays on LCD
- **Input Handler**: `input_handler.c/h` - Joystick (ADC) and button input with debouncing
- **LCD Driver**: `st7789_lcd.c/h` - 320x240 ST7789 display driver
- **Build System**: CMake-based, targets RP2350 (Pico2)

### Hardware (Pico2)
- Joystick X-axis: GPIO 27 (ADC1)
- Joystick Y-axis: GPIO 26 (ADC0)
- Joystick button: GPIO 16
- KEY1: GPIO 2, KEY2: GPIO 3, KEY4: GPIO 15
- ST7789 LCD: SPI (320x240 resolution)
- ADC: 12-bit (0-4095 range), thresholds: 1500-2600 for center deadzone

### MicroPython Implementation (pico/microPython/)

- **Main**: `main.py` - Menu system with splash screen
- **Input Handler**: `input_handler.py` - Edge detection for joystick/buttons
- **Menus**: Go Fly, Bluetooth, Gyro Offset

## Development Commands

### Raspberry Pi

```bash
# Environment setup
python3 -m venv env
source env/bin/activate
pip install -r requirements.txt

# Run main application
cd rpi
python3 main.py

# Debug scripts
cd rpi
python3 debug/DebugGPS.py        # Test GPS
python3 debug/DebugCamera.py     # Test camera
python3 debug/DebugAdxl345.py    # Test accelerometer
python3 debug/key_demo.py        # Test joystick
```

### Pico2 (C)

```bash
# Build
cd pico/c/build
cmake -DPICO_BOARD=pico2 ..
make pico_telemetry_receiver -j4

# Flash
cp pico_telemetry_receiver.uf2 /Volumes/RP2350/

# Monitor
screen /dev/cu.usbmodem14101 115200
```

### Simulator/Testing

```bash
# Send telemetry to Pico
python3 Simulator/send_telemetry.py

# Monitor Pico serial output
python3 Simulator/monitor_serial.py
```

### Key Dependencies

- **PIL (Pillow)**: Image processing and drawing
- **spidev**: SPI communication with display
- **gpiozero**: GPIO control for buttons/sensors
- **picamera2**: Camera interface
- **opencv-python**: Computer vision processing
- **pyserial**: GPS serial communication
- **smbus2**: I2C communication for sensors
- **numpy**: Numerical operations

## Code Patterns

### Raspberry Pi Menu Pattern
Each menu follows this structure:
1. Create PIL Image with black background
2. Use ImageDraw for text/graphics
3. Rotate image 270° before displaying
4. Handle joystick input in while loop
5. Return to main menu on exit

### Raspberry Pi Hardware Interaction
- Use `lcd.digital_read()` for button states
- Check for state transitions (0 and last_state == 1)
- Apply 0.1s sleep for debouncing

### Pico2 C Input Pattern
- Call `input_init()` once at startup
- Call `input_read(&state)` in main loop
- Use `input_just_pressed_*()` functions for edge detection
- Built-in 50ms debouncing

### GPS Data Handling
- Parse NMEA sentences (specifically GPGGA)
- Convert latitude/longitude from DDMM.MMMM to decimal degrees
- Handle fix quality validation before displaying coordinates
