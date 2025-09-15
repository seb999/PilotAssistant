"""
Pico2 Mirror Main
Receives framebuffer data from Raspberry Pi 5 over UART
Displays on Waveshare 1.3" LCD (ST7789)
Handles joystick and buttons with edge detection
"""

from machine import Pin, UART
import time
from st7789 import ST7789  # Your ST7789 driver
from input_handler import InputHandler

print("Pico2 Mirror Starting...")

# -----------------------
# Status LED
# -----------------------
led = Pin(25, Pin.OUT)
led.on()
print("LED initialized")

# -----------------------
# Initialize UART
# -----------------------
uart = UART(0, baudrate=115200, tx=Pin(0), rx=Pin(1))
print("UART initialized")

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
    for color in [0xF800, 0x07E0, 0x001F, 0x0000]:
        display.fill(color)
        time.sleep(0.5)
    
    display_available = True
except Exception as e:
    print(f"Display not available: {e}")
    display_available = False

print("Pico2 ready for communication")

# -----------------------
# Communication buffers
# -----------------------
buffer = b""
FRAME_PREFIX = b"FRAME:240x240:"

def display_splash_logo():
    """Display simple splash logo on the Pico2 display"""
    if not display_available:
        return

    try:
        # Fill with dark blue background
        display.fill(0x001F)  # Dark blue

        # Create simple "PILOT ASSISTANT" pattern
        # White rectangle in center
        for y in range(80, 160):
            for x in range(60, 180):
                if (y == 80 or y == 159 or x == 60 or x == 179):
                    display.pixel(x, y, 0xFFFF)  # White border
                elif 85 < y < 155 and 65 < x < 175:
                    display.pixel(x, y, 0xF800)  # Red interior

        print("Simple splash logo displayed")

    except Exception as e:
        print(f"Error displaying splash: {e}")

# -----------------------
# Main loop
# -----------------------
# -----------------------
# Main loop
# -----------------------
while True:
    try:
        # -----------------------
        # Handle incoming UART data
        # -----------------------
        if uart.any():
            data = uart.read()
            if data:
                led.toggle()
                
                buffer += data

                # Look for complete messages (lines ending with \n)
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    line_str = line.decode('utf-8', errors='ignore').strip()

                    # Handle different message types
                    if line.startswith(FRAME_PREFIX) and display_available:
                        # Framebuffer data
                        frame_data = line[len(FRAME_PREFIX):]

                        # Expect 240*240*2 bytes = 115200
                        if len(frame_data) == 115200:
                            try:
                                display.show_image(frame_data)
                            except Exception as e:
                                print(f"Error showing frame: {e}")

                    elif line_str == "SPLASH":
                        # Splash command
                        print("Received SPLASH command")
                        display_splash_logo()

                    elif line_str.startswith("CMD:"):
                        # Other commands (for future expansion)
                        cmd = line_str[4:]
                        print(f"Received command: {cmd}")
                        # Handle other commands here

                # Send acknowledgment back
                uart.write(b"PICO_ACK\n")
        
        # -----------------------
        # Read button/joystick inputs and send over UART
        # -----------------------
        changes = inputs.read_inputs()
        for name, state in changes:
            if state == 0:  # pressed
                print(f"{name} pressed")
                uart.write(f"BTN:{name}:PRESSED\n".encode())
            else:  # released
                print(f"{name} released")
                uart.write(f"BTN:{name}:RELEASED\n".encode())
        
        # Short delay
        time.sleep_ms(10)
    
    except Exception as e:
        print(f"Main loop error: {e}")
        time.sleep_ms(100)


