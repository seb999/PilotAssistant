# PilotAssistant

A dual-platform pilot assistant system with GPS tracking, sensor readings, and flight assistance features.

## Platforms

- **Raspberry Pi** (`rpi/`): Full-featured system with Python, LCD display, GPS, camera, and sensors
- **Pico2** (`pico/`): Lightweight RP2350-based system with C and MicroPython implementations

## Quick Start

### Raspberry Pi

```bash
# Setup environment
python3 -m venv env
source env/bin/activate
pip install -r requirements.txt

# Run
cd rpi
python3 main.py
```

### Pico2 (C)

```bash
# Build
cd pico/c/build
cmake -DPICO_BOARD=pico2 ..
make -j4

# Flash (hold BOOTSEL, plug USB, release BOOTSEL)
cp pico_telemetry_receiver.uf2 /Volumes/RP2350/
```

### Testing

#### Simulator Tools

```bash
# Send telemetry to Pico
python3 Simulator/send_telemetry.py

# Monitor Pico output
python3 Simulator/monitor_serial.py
```

#### Hardware Testing (Raspberry Pi C)

Test individual hardware components:

```bash
cd rpi/c/debug

# Test GPS module
gcc gps.c -o gps && ./gps

# Test ADXL345 accelerometer
gcc adxl345.c -o adxl345 && ./adxl345

# Test ST7789 LCD display
gcc lcd.c -o lcd && ./lcd

# Test camera (libcamera C++ version)
g++ camera_libcamera.cpp -o camera_libcamera $(pkg-config --cflags --libs libcamera) && ./camera_libcamera

# Test camera to LCD integration
gcc cam_to_lcd.c -o cam_to_lcd && ./cam_to_lcd

# Test Pico2 command receiver (monitor button/joystick commands from Pico)
gcc pico_commands.c -o pico_commands && ./pico_commands
```

All test programs exit with Ctrl+C.

## Project Structure

```
/
├── rpi/              # Raspberry Pi implementation
│   ├── main.py       # Main entry point
│   ├── library/      # LCD, GPIO, config
│   ├── menu/         # Menu pages
│   ├── service/      # Background services
│   └── debug/        # Test scripts
├── pico/             # Pico2 implementations
│   ├── c/            # C (telemetry, inputs)
│   └── microPython/  # MicroPython (menu system)
└── Simulator/        # Testing tools
```

## Features

### Raspberry Pi
- 240x240 ST7789 LCD display
- GPS tracking with NMEA parsing
- ADXL345 accelerometer
- Pi Camera integration
- Traffic awareness
- Bluetooth connectivity

### Pico2
- 320x240 ST7789 LCD display
- USB telemetry receiver
- Joystick input (ADC-based, 4-way)
- Button inputs with debouncing
- Menu system with icons

## Documentation

See [CLAUDE.md](CLAUDE.md) for detailed architecture and development guidelines.
