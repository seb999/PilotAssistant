# Raspberry Pi C Implementation Status

## ✅ Complete - Ready to Test on Hardware

### Available Debug Programs

1. **debug_adxl345** - ADXL345 Accelerometer Test
   - Tests I2C communication
   - Displays X, Y, Z acceleration in real-time
   - File: `debug/adxl345.c`

2. **debug_gps** - GPS Module Test
   - Tests UART serial communication
   - Displays raw NMEA sentences
   - Automatic GPS power control via GPIO
   - File: `debug/gps.c`

3. **debug_lcd** - ST7789 LCD Display Test
   - Tests SPI communication
   - Tests GPIO control (RST, DC, BL pins)
   - Comprehensive display tests:
     - Color fills (red, green, blue)
     - Text rendering (normal and scaled)
     - Graphics primitives (rectangles, lines, circles)
   - File: `debug/lcd.c`
   - Driver: `src/st7789_rpi.c` + `include/st7789_rpi.h`

## Hardware Configuration

### LCD (ST7789 - 240x240)
- **SPI Device:** `/dev/spidev0.0` @ 40 MHz
- **RST Pin:** GPIO 27
- **DC Pin:** GPIO 25
- **BL Pin:** GPIO 24
- **Resolution:** 240x240 pixels
- **Color Format:** RGB565

### ADXL345 (Accelerometer)
- **I2C Bus:** Bus 1
- **Address:** 0x53
- **Range:** ±2g

### GPS Module
- **Serial Port:** `/dev/ttyAMA0`
- **Baud Rate:** 9600
- **Enable Pin:** GPIO 17
- **Protocol:** NMEA

## Build Instructions

```bash
cd rpi/c/build
cmake ..
make

# All three programs will be built:
# - debug_adxl345
# - debug_gps
# - debug_lcd
```

## Testing on Raspberry Pi

### 1. Test Accelerometer
```bash
./debug_adxl345
# Should display X, Y, Z values in g-forces
# Press Ctrl+C to exit
```

### 2. Test GPS
```bash
./debug_gps
# Should display NMEA sentences as they arrive
# Check GPS LED is blinking
# Press Ctrl+C to exit
```

### 3. Test LCD
```bash
./debug_lcd
# Should show:
# 1. Color fills (red→green→blue→black)
# 2. Text in various sizes
# 3. Rectangles, lines, and circles
# 4. Final "PILOT ASSISTANT" message
# Runs automatically, exits after 5 seconds
```

## Prerequisites

Enable required interfaces on Raspberry Pi:

```bash
sudo raspi-config
```

1. **I2C** (for ADXL345):
   - Interface Options → I2C → Enable

2. **SPI** (for LCD):
   - Interface Options → SPI → Enable

3. **Serial Port** (for GPS):
   - Interface Options → Serial Port
   - Login shell: No
   - Serial hardware: Yes

Then reboot:
```bash
sudo reboot
```

## Code Structure

```
rpi/c/
├── include/
│   └── st7789_rpi.h       # LCD driver header
├── src/
│   └── st7789_rpi.c       # LCD driver implementation
├── debug/
│   ├── adxl345.c          # Accelerometer test
│   ├── gps.c              # GPS test
│   └── lcd.c              # LCD test
├── build/                  # Build directory
└── CMakeLists.txt         # Build configuration
```

## Features

### ST7789 LCD Driver (st7789_rpi.c/h)
- ✅ Full initialization sequence
- ✅ Hardware reset via GPIO
- ✅ Backlight control
- ✅ Fast SPI communication (40 MHz)
- ✅ Color fills
- ✅ Pixel drawing
- ✅ Line drawing (Bresenham algorithm)
- ✅ Circle drawing (Midpoint algorithm)
- ✅ Text rendering (5x7 font, ASCII 32-90)
- ✅ Scaled text (1x, 2x, 3x, etc.)
- ✅ RGB565 color format

### All Programs
- ✅ Clean signal handling (Ctrl+C)
- ✅ Error checking and reporting
- ✅ GPIO cleanup on exit
- ✅ No external dependencies (pure Linux APIs)

## Next Steps

1. Transfer code to Raspberry Pi
2. Build all programs
3. Test each component individually
4. Verify all hardware is working
5. Ready to build full application!

## Notes

- All code uses standard Linux APIs (spidev, i2c-dev, sysfs GPIO, termios)
- No Python dependencies needed
- Fast and lightweight
- Independent from Pico implementation (simpler maintenance)
