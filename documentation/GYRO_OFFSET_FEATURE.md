# Gyro Offset Feature

## Overview
The GYRO OFFSET feature allows users to calibrate the attitude indicator for non-flat mounting surfaces in the aircraft. Adjustments are made on the Pico2 and sent to the Raspberry Pi HUD in real-time.

## How It Works

### Pico2 Side (Display & Input)
- When "GYRO OFFSET" is selected from the menu, a dedicated calibration screen appears
- Users adjust offsets using the joystick:
  - **UP/DOWN**: Adjust pitch offset (±degrees)
  - **LEFT/RIGHT**: Adjust roll/bank offset (±degrees)
  - **PRESS**: Exit and return to menu
- Current offset values are displayed in large yellow text
- Changes are sent immediately to the Raspberry Pi

### Raspberry Pi Side (Processing)
- Receives offset commands via USB serial from Pico2
- Command format: `CMD:OFFSET:PITCH:5` or `CMD:OFFSET:ROLL:-3`
- Applies offsets to sensor readings in real-time
- Offsets are added to calculated pitch/roll before display

## Command Protocol

### Commands Sent from Pico2 to RPI:
```
CMD:OFFSET_MODE           - Entering offset adjustment mode
CMD:OFFSET:PITCH:<value>  - Set pitch offset (e.g., CMD:OFFSET:PITCH:5)
CMD:OFFSET:ROLL:<value>   - Set roll offset (e.g., CMD:OFFSET:ROLL:-3)
CMD:OFFSET_EXIT           - Exiting offset mode
```

## Implementation Details

### Pico2 Files Modified:
- `/pico/c/main_menu.c` - Updated `action_gyro_offset()` function
  - Interactive offset adjustment screen
  - Real-time display of current offsets
  - Joystick-based adjustment (±1 degree increments)

### Raspberry Pi Files Modified:
- `/rpi/c/src/main.c`:
  - Added `pitch_offset` and `roll_offset` global variables
  - Added `parse_cmd_message()` function to parse offset commands
  - Updated `update_attitude_from_sensor()` to apply offsets
  - Integrated CMD parsing into `process_serial_input()`

## Usage Instructions

1. **Start the system**: Run `./pilot_assistant` on Raspberry Pi
2. **Navigate on Pico2**: Use joystick to select "GYRO OFFSET"
3. **Adjust offsets**:
   - UP/DOWN adjusts pitch (compensates for forward/backward tilt)
   - LEFT/RIGHT adjusts roll (compensates for left/right tilt)
4. **Monitor on HUD**: Watch the attitude indicator adjust in real-time
5. **Exit**: Press joystick button when calibration is complete

## Code Locations

### Key Functions:
- **Pico2**: `action_gyro_offset()` in [main_menu.c:109-196](../pico/c/main_menu.c#L109-L196)
- **RPI**: `parse_cmd_message()` in [main.c:475-502](../rpi/c/src/main.c#L475-L502)
- **RPI**: Offset application in [main.c:433-434](../rpi/c/src/main.c#L433-L434)

## Building

### Pico2:
```bash
cd pico/c/build
cmake -DPICO_BOARD=pico2 ..
make main_menu -j4
cp main_menu.uf2 /path/to/pico2/
```

### Raspberry Pi:
```bash
cd rpi/c
gcc -O2 -Wall -Wextra -o pilot_assistant src/main.c src/st7789_rpi.c src/adxl345.c src/gps.c -I./include -lgpiod -lm
```

## Notes
- Offsets are stored in memory only (reset on power cycle)
- Offsets are applied after sensor reading but before display
- Range: ±180 degrees (practical range: ±20 degrees for typical installations)
- Adjustment increment: 1 degree per joystick press
