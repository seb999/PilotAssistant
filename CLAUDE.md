# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PilotAssistant is a Raspberry Pi-based pilot assistant system with a 240x240 ST7789 LCD display and joystick controls. The system provides GPS tracking, sensor readings, and flight assistance features through an interactive menu interface.

## Architecture

### Core Components

- **Main Entry Point**: `main.py` - Contains the main menu system with 5 options (SETUP, ACTIVATE, GPS, STATUS, SHUTDOWN)
- **Display System**: `library/ST7789.py` - ST7789 LCD display driver with 240x240 resolution
- **Hardware Configuration**: `library/config.py` - GPIO pin definitions, SPI setup, and hardware interfaces
- **State Management**: `library/state.py` - Shared state for pitch/bank offsets
- **Menu Modules**: `menu/` directory contains specialized menu pages:
  - `setup_menu.py` - Pitch and bank calibration interface
  - `gps_menu.py` - GPS coordinate display and NMEA parsing
  - `stream_menu.py` - Camera/streaming functionality

### Hardware Interface

The system uses the following GPIO pins (defined in `library/config.py`):
- Joystick: UP(6), DOWN(19), LEFT(5), RIGHT(26), PRESS(13)
- Additional keys: KEY1(21), KEY2(20), KEY3(16)
- GPS communication via serial port
- ADXL345 accelerometer via I2C
- Camera via Pi Camera interface

### Display System

- 240x240 pixel ST7789 LCD with SPI communication
- Images rotated 270° for proper orientation
- Menu system with 5 main options positioned at specific coordinates
- Color scheme: CYAN for text/selection, BLACK backgrounds, MAGENTA for titles

## Development Commands

### Environment Setup
```bash
# Create and activate virtual environment
python3 -m venv env
source env/bin/activate

# Install dependencies
pip install -r requirements.txt
```

### Running the Application
```bash
# Main application
python3 main.py

# Debug scripts (in debug/ directory)
python3 debug/DebugGPS.py        # Test GPS functionality
python3 debug/DebugCamera.py     # Test camera
python3 debug/DebugAdxl345.py    # Test accelerometer
python3 debug/key_demo.py        # Test joystick inputs
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

### Menu Creation Pattern
Each menu follows this structure:
1. Create PIL Image with black background
2. Use ImageDraw for text/graphics
3. Rotate image 270° before displaying
4. Handle joystick input in while loop
5. Return to main menu on exit

### Hardware Interaction
- Use `lcd.digital_read()` for button states
- Check for state transitions (0 and last_state == 1)
- Apply 0.1s sleep for debouncing

### GPS Data Handling
- Parse NMEA sentences (specifically GPGGA)
- Convert latitude/longitude from DDMM.MMMM to decimal degrees
- Handle fix quality validation before displaying coordinates