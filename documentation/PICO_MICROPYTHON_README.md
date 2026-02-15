# MicroPython Implementation

This directory contains the original MicroPython implementation for the Raspberry Pi Pico display controller.

## Contents

- `main.py` - Main menu system with ST7789 LCD interface
- `st7789.py` - ST7789 display driver for MicroPython
- `input_handler.py` - Joystick and button input handling
- `boot.py` - Boot configuration for Pico
- `splash_loader.py` - Splash screen loader
- `menu/` - Menu modules:
  - `artificial_horizon.py` - Artificial horizon display
  - `bluetooth_menu.py` - Bluetooth configuration
  - `go_fly_menu.py` - Flight mode interface
- `upload_to_pico.sh` - Upload script for deploying to Pico
- `Backup/` - Backup copies of core files

## Assets

- `splashPico.png` - Full-resolution splash image
- `splashPico_RGB565.bin` - Pre-converted RGB565 binary splash
- `splash.png` - Original splash image

## Note

This implementation has been superseded by a C-language implementation for improved performance. The C implementation is located in the parent `/pico` directory.
