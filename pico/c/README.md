# Pico C Implementation

This directory contains C implementations for the Raspberry Pi Pico2 with ST7789 LCD display.

**Note:** This implementation now uses the shared ST7789 driver from `common/st7789/` which is also used by the Raspberry Pi implementation for consistency and code reuse.

## Available Programs

1. **menu_system** - Interactive menu with joystick navigation, radar display, and telemetry
2. **input_test** - Tests joystick and button inputs

## Features

### Telemetry Receiver
- Receives JSON telemetry data via USB CDC serial
- Displays data on 320x240 ST7789 LCD
- Parses own aircraft position, altitude, and attitude (pitch, roll, yaw)
- Tracks nearby traffic aircraft
- LED indicator for successful data reception

### Input Test
- Tests analog joystick (ADC-based)
- Tests digital buttons (KEY1, KEY2, KEY4)
- Visual feedback on LCD
- Edge detection and debouncing
- Real-time status display

## Prerequisites

1. **Pico SDK** - Install the Raspberry Pi Pico SDK
   ```bash
   # Clone the SDK (if not already installed)
   cd ~/
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init

   # Set environment variable
   export PICO_SDK_PATH=~/pico-sdk
   ```

2. **Build Tools**
   ```bash
   # macOS
   brew install cmake gcc-arm-embedded

   # Linux
   sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
   ```

## Building

1. Set the Pico SDK path (add to your `~/.zshrc` or `~/.bashrc`):
   ```bash
   export PICO_SDK_PATH=~/pico-sdk
   ```

2. Create build directory and compile:
   ```bash
   cd /Users/sebastien/_Git/RasberryPi/PilotAssistant/pico/c
   mkdir build
   cd build
   cmake ..
   make
   ```

3. The output files will be created in the `build/` directory:
   - `menu_system.uf2` - Main menu system with radar
   - `input_test.uf2` - Input handler test program

## Flashing to Pico

### Method 1: Bootloader Mode (Recommended for first flash)

1. Hold down the BOOTSEL button on the Pico
2. While holding BOOTSEL, connect the Pico to your Mac via USB
3. Release BOOTSEL - the Pico will appear as a USB drive named "RPI-RP2"
4. Copy the UF2 file to the drive:
   ```bash
   cp build/pico_telemetry_receiver.uf2 /Volumes/RPI-RP2/
   ```
5. The Pico will automatically reboot and run the new firmware

### Method 2: Using picotool (for subsequent flashes)

```bash
# Install picotool
brew install picotool

# Flash the Pico
picotool load build/pico_telemetry_receiver.uf2 -f
picotool reboot
```

## Usage

### Testing Input Handler (Recommended First Step)

1. **Flash the input_test.uf2** firmware to test joystick and buttons:
   ```bash
   cp build/input_test.uf2 /Volumes/RPI-RP2/
   ```

2. **Connect via USB** and monitor output:
   ```bash
   screen /dev/cu.usbmodem14201 115200
   ```

3. **Test the inputs**:
   - Move the joystick UP, DOWN, LEFT, RIGHT
   - Press the joystick button (center press)
   - Press KEY1, KEY2, KEY4 buttons
   - Watch the LCD display and serial output for feedback

4. **Pin Configuration**:
   - Joystick X-axis: GPIO 27 (ADC1)
   - Joystick Y-axis: GPIO 26 (ADC0)
   - Joystick button: GPIO 16
   - KEY1: GPIO 2
   - KEY2: GPIO 3
   - KEY4: GPIO 15

### Using Telemetry Receiver

1. **Flash the telemetry firmware** to your Pico using the instructions above

2. **Connect the Pico** via USB to your Mac - it should now appear as a serial device:
   ```bash
   ls /dev/cu.usbmodem*
   # Should show something like: /dev/cu.usbmodem14201
   ```

3. **Update the serial port** in `Simulator/send_telemetry.py`:
   ```python
   # Change this line to match your device:
   SERIAL_PORT = "/dev/cu.usbmodem14201"  # Use the actual device name from step 2
   ```

4. **Monitor the Pico output** (optional - to see received telemetry):
   ```bash
   # In one terminal, monitor the Pico
   screen /dev/cu.usbmodem14201 115200
   ```

5. **Run the telemetry sender**:
   ```bash
   # In another terminal
   cd /Users/sebastien/_Git/RasberryPi/PilotAssistant
   source env/bin/activate
   python3 Simulator/send_telemetry.py
   ```

6. You should see:
   - The sender script printing "Sent: ..." messages
   - The Pico LED blinking with each received message
   - (If monitoring) The Pico displaying parsed telemetry data

## Troubleshooting

### Pico doesn't appear as USB device
- Make sure you're using a USB data cable (not charge-only)
- Try a different USB port
- Verify the firmware flashed successfully (LED should blink 3 times on startup)

### "Permission denied" when accessing serial port
```bash
# macOS - no special permissions needed, but ensure no other app is using the port
# Close any screen/minicom sessions

# Linux - add user to dialout group
sudo usermod -a -G dialout $USER
# Then log out and back in
```

### No data received
1. Check that the serial port in `send_telemetry.py` matches the actual device
2. Verify baud rate is 115200 in both sender and receiver
3. Check USB cable connection
4. Monitor the Pico output to see if it's receiving garbled data

### Build errors
- Ensure `PICO_SDK_PATH` environment variable is set correctly
- Update Pico SDK: `cd $PICO_SDK_PATH && git pull`
- Delete `build/` directory and rebuild from scratch

## Code Structure

- **main_menu.c** - Main menu system with radar display
- **main_input_test.c** - Input testing program
- **input_handler.c/h** - Joystick and button input with debouncing
- **menu.c/h** - Menu navigation system
- **telemetry_parser.c/h** - JSON parsing for telemetry data
- **st7789_icons.c/h** - Pico-specific icon drawing functions
- **CMakeLists.txt** - Build configuration
- **../../common/st7789/** - Shared ST7789 LCD driver (used by both Pico and RPi)

## Data Format

The Pico expects JSON telemetry in this format:
```json
{
  "own": {
    "lat": 37.7749,
    "lon": -122.4194,
    "alt": 5000,
    "pitch": 1.2,
    "roll": 3.4,
    "yaw": 45.6
  },
  "traffic": [
    {"id": "T1", "lat": 37.78, "lon": -122.42, "alt": 5200},
    {"id": "T2", "lat": 37.77, "lon": -122.41, "alt": 4800}
  ]
}
```

## LED Behavior

- **3 quick blinks on startup** - System ready
- **Brief blink** - Telemetry message successfully received and parsed
- **Solid/erratic** - Check for errors in serial communication
