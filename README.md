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

```bash
# Send telemetry to Pico
python3 Simulator/send_telemetry.py

# Monitor Pico output
python3 Simulator/monitor_serial.py
```

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
