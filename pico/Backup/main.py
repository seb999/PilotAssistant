"""
Pico2 Mirror Main over USB
Receives framebuffer data from Raspberry Pi over USB serial
Displays on Waveshare 1.3" LCD (ST7789)
Handles joystick and buttons with edge detection
"""

from machine import Pin
import time
import sys  # USB serial
from st7789 import ST7789
from input_handler import InputHandler
import select

print("Pico2 Mirror Starting (USB Mode)...")

# -----------------------
# Status LED
# -----------------------
led = Pin(25, Pin.OUT)
led.on()
print("LED initialized")

# -----------------------
# Initialize InputHandler
# -----------------------
default_pins = {
    'up': 2,
    'press': 3,
    'down': 18,
    'left': 16,
    'right': 20,
    'key1': 15,
    'key2': 17,
    'key3': 19,
    'key4': 21
}
inputs = InputHandler(pin_map=default_pins)
print("InputHandler initialized")

# -----------------------
# Initialize Display
# -----------------------
try:
    display = ST7789()
    print("Display initialized successfully")

    # Test display with colors
    for color in [0xF800, 0x07E0, 0x001F]:
        display.fill(color)
        time.sleep(0.5)

    display_available = True
except Exception as e:
    print(f"Display not available: {e}")
    display_available = False

print("Pico2 ready for USB communication")

# -----------------------
# Communication buffers
# -----------------------
buffer = b""
FRAME_HEADER = b"FRAME_START\n"
FRAME_SIZE = 240 * 240 * 2  # 115200 bytes

# -----------------------
# Main loop
# -----------------------
while True:
    try:
        # -----------------------
        # Handle incoming USB serial data
        # -----------------------
       
           

        # -----------------------
        # Read button/joystick inputs and send over USB
        # -----------------------
        changes = inputs.read_inputs()
        for name, state in changes:
            if state == 0:  # pressed
                print(f"{name} pressed")
                sys.stdout.buffer.write(f"BTN:{name}:PRESSED\n".encode())
            else:  # released
                print(f"{name} released")
                sys.stdout.buffer.write(f"BTN:{name}:RELEASED\n".encode())

        # Short delay
        time.sleep_ms(10)

    except Exception as e:
        print(f"Main loop error: {e}")
        time.sleep_ms(100)


