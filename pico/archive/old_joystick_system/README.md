# Archived: Old Joystick-Based System

This directory contains the obsolete joystick/button-based navigation system that was replaced with the touchscreen-based interface.

## Archived Files

### Main Application (Old)
- `main_menu.c` - Old menu system with joystick navigation and USB serial communication
- `CMakeLists.txt` - Old build configuration

### Input System (Joystick)
- `input_handler.c/h` - Joystick and button input handling
- `main_input_test.c` - Joystick testing utility
- `main_command_sender.c` - Sends joystick commands to Raspberry Pi via USB

### Old Feature Implementations
- `attitude_indicator.c/h` - Old attitude indicator implementation
- `mag_calibration.c/h` - Old magnetometer calibration

## Why Archived?

The system was migrated to:
- **Touchscreen input** (`xpt2046_touch.c`) instead of joystick
- **WiFi communication** instead of USB serial
- **Bluetooth support** for pilot headphone alerts
- **Larger touch-friendly buttons** (80×80 pixels)
- **Modern menu system** in `pico/c/src/main_menu.c`

## Date Archived
April 28, 2026

## Can These Be Deleted?
Yes, these files are preserved for reference only. The new system in `/pico/c/` is fully functional and replaces all this functionality.
