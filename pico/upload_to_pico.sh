#!/bin/bash

# Upload script for Pico2 Pilot Assistant
# Uploads all necessary files to the Raspberry Pi Pico2

PICO_DEV=/dev/cu.usbmodem1431201
PROJECT_DIR=/Users/sebastien/_Git/RasberryPi/PilotAssistant/pico

echo "Starting upload to Pico2..."
echo "Device: $PICO_DEV"
echo "Project directory: $PROJECT_DIR"

cd "$PROJECT_DIR"

echo "Creating menu directory on Pico..."
mpremote connect $PICO_DEV mkdir menu 2>/dev/null || true

echo "Uploading main files..."
mpremote connect $PICO_DEV cp main.py :
mpremote connect $PICO_DEV cp st7789.py :
mpremote connect $PICO_DEV cp input_handler.py :
mpremote connect $PICO_DEV cp splash_loader.py :

echo "Uploading splash screen files..."
mpremote connect $PICO_DEV cp splash_compact.py :
mpremote connect $PICO_DEV cp splash_chunked.py :
mpremote connect $PICO_DEV cp simple_splash.py :
echo "Uploading raw splash image..."
mpremote connect $PICO_DEV cp splash_240x240.raw :

echo "Uploading menu files..."
mpremote connect $PICO_DEV cp menu/bluetooth_menu.py :menu/
mpremote connect $PICO_DEV cp menu/go_fly_menu.py :menu/

echo "Resetting Pico..."
mpremote connect $PICO_DEV reset

echo "Upload complete!"
echo "Check the Pico display and serial output for any errors."