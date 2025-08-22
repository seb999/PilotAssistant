# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Setup and Dependencies

Set up the development environment using:

```bash
python3 -m venv env
source env/bin/activate
```

This project requires:
- Python 3 with standard libraries (PIL, time, logging)
- Hardware-specific libraries: spidev, gpiozero, RPi.GPIO
- Camera support: picamera2, cv2 (OpenCV)
- Additional: serial (for GPS debugging)

## Running the Application

The main application entry point is `main.py`. It displays a menu-driven interface on an ST7789 240x240 LCD display with joystick navigation.

```bash
python3 main.py
```

For hardware testing and debugging, use files in the `debug/` directory:
- `DebugGPS.py` - GPS module testing 
- `DebugCamera.py` - Camera functionality testing
- `DebugAdxl345.py` - Accelerometer testing

## Architecture

### Hardware Integration
- **Display**: ST7789 240x240 LCD with SPI interface
- **Input**: Hardware joystick with directional buttons (up/down/left/right/center) plus additional keys (KEY1/KEY2/KEY3)
- **Camera**: Picamera2 integration for live video streaming
- **Sensors**: GPS and accelerometer support

### Code Structure

**Main Components:**
- `main.py` - Application entry point with menu system and joystick navigation
- `library/` - Hardware abstraction layer
  - `ST7789.py` - LCD display driver extending config.RaspberryPi
  - `config.py` - GPIO pin definitions and hardware interface (Waveshare-based)
  - `state.py` - Global state management for pitch/bank offsets
- `menu/` - Menu system implementation
  - `setup_menu.py` - Configuration interface for pitch/bank calibration
  - `stream_menu.py` - Live camera streaming display
- `debug/` - Hardware testing utilities

**Key Patterns:**
- Hardware abstraction through `config.RaspberryPi` class using gpiozero
- Image-based UI using PIL for drawing and ST7789 for display
- State management through simple global variables in `library.state`
- Button debouncing with state tracking variables
- All graphics rotated 270° for landscape display orientation

### GPIO Configuration

The system uses specific GPIO pins for joystick input:
- UP: GPIO 6, DOWN: GPIO 19, LEFT: GPIO 5, RIGHT: GPIO 26, CENTER: GPIO 13
- Additional keys: KEY1: GPIO 21, KEY2: GPIO 20, KEY3: GPIO 16
- Pull-up resistors are configured in software via gpiozero

Note: This is a Raspberry Pi hardware project designed for embedded display and sensor applications.