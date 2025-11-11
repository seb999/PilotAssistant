# Raspberry Pi C Implementation

This directory contains the C implementation of the PilotAssistant system for Raspberry Pi.

## Directory Structure

```
c/
├── src/          # Main source files
├── include/      # Header files
├── debug/        # Debug/test programs
├── build/        # Build output directory
└── CMakeLists.txt
```

## Building

### On Raspberry Pi

```bash
# Create build directory
cd rpi/c/build
cmake ..
make

# Or build specific targets
make debug_adxl345
```

### Cross-Compilation from macOS/Linux

The code requires Linux I2C headers and must be built on the Raspberry Pi or using a cross-compiler. To build on macOS, you would need to set up a Raspberry Pi cross-compilation toolchain.

For development/testing on macOS, you can:
1. Copy the code to your Raspberry Pi via `scp` or git
2. SSH into the Pi and build there
3. Use VSCode Remote SSH extension to develop directly on the Pi

## Debug Programs

### ADXL345 Accelerometer Test

Tests the ADXL345 accelerometer and displays X, Y, Z acceleration values.

```bash
# Build
cd rpi/c/build
make debug_adxl345

# Run
./debug_adxl345
```

**Requirements:**
- I2C must be enabled on the Raspberry Pi
- ADXL345 connected to I2C bus 1 at address 0x53

**Controls:**
- Press `Ctrl+C` to exit

### GPS Module Test

Tests GPS module communication and displays raw NMEA sentences.

```bash
# Build
cd rpi/c/build
make debug_gps

# Run
./debug_gps
```

**Requirements:**
- GPS module connected to `/dev/ttyAMA0` (UART)
- Serial console must be disabled in raspi-config
- GPS enable pin on GPIO 17

**Features:**
- Automatic GPS power-on via GPIO 17
- Raw NMEA sentence display with timestamps
- Connection diagnostics
- Line count statistics

**Controls:**
- Press `Ctrl+C` to exit

### ST7789 LCD Test

Tests the ST7789 240x240 LCD display.

```bash
# Build
cd rpi/c/build
make debug_lcd

# Run
./debug_lcd
```

**Requirements:**
- ST7789 LCD connected via SPI0
- RST pin on GPIO 27
- DC pin on GPIO 25
- Backlight pin on GPIO 24
- SPI must be enabled in raspi-config

**Test Suite:**
1. Color fill test (red, green, blue)
2. Text rendering (normal and scaled)
3. Graphics primitives (rectangles, lines, circles)

**Features:**
- 5x7 font with scaling support
- RGB565 color format
- Line and circle drawing

**Controls:**
- Tests run automatically

### Camera Stream to LCD

Captures camera frames and streams them to the LCD display in real-time.

```bash
# Build
cd rpi/c/build
make debug_camera

# Run
./debug_camera
```

**Requirements:**
- Raspberry Pi Camera connected and enabled
- Camera interface must be enabled in raspi-config
- ST7789 LCD connected via SPI0
- `/dev/video0` must be available

**Features:**
- Real-time video streaming to LCD (240x240)
- YUYV to RGB565 color conversion
- FPS counter
- Uses Video4Linux2 (V4L2) API
- Direct memory-mapped buffer access

**Controls:**
- Press `Ctrl+C` to exit

## Hardware Requirements

- Raspberry Pi (any model with I2C, SPI, and Camera support)
- I2C enabled in raspi-config
- SPI enabled in raspi-config
- Camera interface enabled in raspi-config

## Future Programs

Additional debug programs will be added for:
- GPS (serial communication)
- ST7789 LCD (SPI display)
- Camera
- Joystick/buttons
