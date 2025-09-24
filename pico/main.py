"""
Pico2 Dual Mode System
- Displays local pilot assistant menu on LCD
- Sends button presses to Raspberry Pi over USB serial
- Handles joystick and buttons with edge detection
"""

from machine import Pin
import time
import sys  # USB serial
from st7789 import ST7789
from input_handler import InputHandler
from menu.bluetooth_menu import display_bluetooth_menu
from menu.go_fly_menu import display_go_fly_menu
from splash_loader import show_splash_screen

print("Pico2 Dual Mode Starting...")

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
    display_available = True
except Exception as e:
    print(f"Display not available: {e}")
    display_available = False

print("Pico2 ready for dual mode operation")

# -----------------------
# Menu System Functions
# -----------------------
def rgb565(r, g, b):
    """Convert RGB888 to RGB565 color format - BGR order for this display"""
    return ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)

def draw_char(display, char, x, y, color, font_size=1):
    """Draw a single character using direct pixel drawing"""
    # Simple 8x8 bitmap font for basic characters
    font_8x8 = {
        'A': [0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00],
        'B': [0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00],
        'C': [0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00],
        'D': [0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00],
        'E': [0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00],
        'F': [0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00],
        'G': [0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00],
        'H': [0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00],
        'I': [0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00],
        'L': [0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00],
        'M': [0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00],
        'N': [0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00],
        'O': [0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00],
        'P': [0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00],
        'R': [0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00],
        'S': [0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00],
        'T': [0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00],
        'U': [0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00],
        'Y': [0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00],
        ' ': [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
    }

    if char.upper() not in font_8x8:
        return

    bitmap = font_8x8[char.upper()]
    for row in range(8):
        byte_val = bitmap[row]
        for col in range(8):
            if byte_val & (1 << (7 - col)):
                # Draw scaled pixel block using draw_rect for efficiency
                px = x + col * font_size
                py = y + row * font_size
                if font_size == 1:
                    display.draw_pixel(px, py, color)
                else:
                    display.draw_rect(px, py, font_size, font_size, color)

def draw_text_direct(display, text, x, y, color, font_size=1):
    """Draw text directly to display without using frame buffer"""
    char_width = 8 * font_size
    for i, char in enumerate(text):
        char_x = x + (i * char_width)
        if char_x >= display.width:
            break
        draw_char(display, char, char_x, y, color, font_size)


# Menu setup
menu_items = ['Go Fly', 'Bluetooth']
selection_index = -1  # No default selection
last_selection_index = -2  # Track previous selection for partial updates

# Colors
BLACK = rgb565(0, 0, 0)
CYAN = rgb565(0, 255, 255)
MAGENTA = rgb565(255, 0, 255)

print(f"Color values - BLACK: 0x{BLACK:04X}, CYAN: 0x{CYAN:04X}, MAGENTA: 0x{MAGENTA:04X}")

# Menu item positions matching pilot_assistant_system.py
label_positions = [
    (5, 65, 235, 95),   # Go Fly
    (5, 100, 235, 130)  # Bluetooth
]

def draw_menu_item(item_index, selected=False):
    """Draw a single menu item efficiently"""
    if not display_available or item_index >= len(menu_items):
        return

    rect = label_positions[item_index]
    item = menu_items[item_index]
    menu_font_size = 2

    if selected:
        # Selected item - draw CYAN rectangle background
        rect_width = rect[2] - rect[0]
        rect_height = rect[3] - rect[1]
        display.draw_rect(rect[0], rect[1], rect_width, rect_height, CYAN)

        # Draw BLACK text on CYAN background with vertical centering
        text_height = 8 * menu_font_size
        text_y = rect[1] + (rect_height - text_height) // 2 - 2
        draw_text_direct(display, item, rect[0] + 5, text_y, BLACK, menu_font_size)
    else:
        # Unselected item - clear background and draw CYAN text
        rect_width = rect[2] - rect[0]
        rect_height = rect[3] - rect[1]
        display.draw_rect(rect[0], rect[1], rect_width, rect_height, BLACK)  # Clear background

        text_y = rect[1] + 5  # Small offset from top
        draw_text_direct(display, item, rect[0] + 5, text_y, CYAN, menu_font_size)

def init_menu_display():
    """Initialize the menu display (full redraw)"""
    global last_selection_index
    if not display_available:
        return

    # Clear screen with black background (0x0000)
    display.clear(0x0000)

    # Title "PILOT ASSISTANT" - centered with proper positioning
    title_text = "PILOT ASSISTANT"
    title_font_size = 2  # Larger title font
    title_x = (display.width - len(title_text) * 8 * title_font_size) // 2  # Center the title
    draw_text_direct(display, title_text, title_x, 8, MAGENTA, title_font_size)

    # Draw all menu items
    for i in range(len(menu_items)):
        draw_menu_item(i, i == selection_index)

    last_selection_index = selection_index

def update_menu_display():
    """Update the menu display with partial updates for speed"""
    global last_selection_index
    if not display_available:
        return

    # Only update if selection changed
    if selection_index == last_selection_index:
        return

    # Redraw previously selected item as unselected
    if last_selection_index >= 0 and last_selection_index < len(menu_items):
        draw_menu_item(last_selection_index, False)

    # Redraw newly selected item as selected
    if selection_index >= 0 and selection_index < len(menu_items):
        draw_menu_item(selection_index, True)

    last_selection_index = selection_index

# Show splash screen for 2 seconds
if display_available:
    try:
        # Use enhanced splash loader with multiple fallback options
        show_splash_screen(display)

        # Add text overlay on top of image
        splash_text = "PILOT ASSISTANT"
        subtitle_text = "STARTING..."

        splash_font_size = 2
        splash_x = (display.width - len(splash_text) * 8 * splash_font_size) // 2
        splash_y = 200  # Near bottom to overlay on image

        subtitle_x = (display.width - len(subtitle_text) * 8 * splash_font_size) // 2
        subtitle_y = splash_y + 25

        # Draw text with semi-transparent effect by drawing shadow first
        draw_text_direct(display, splash_text, splash_x + 1, splash_y + 1, BLACK, splash_font_size)
        draw_text_direct(display, splash_text, splash_x, splash_y, MAGENTA, splash_font_size)

        draw_text_direct(display, subtitle_text, subtitle_x + 1, subtitle_y + 1, BLACK, splash_font_size)
        draw_text_direct(display, subtitle_text, subtitle_x, subtitle_y, CYAN, splash_font_size)

        print("Splash screen with overlay displayed")

        # Wait for 2 seconds
        time.sleep(2)

    except Exception as e:
        print(f"Error displaying splash: {e}")
        # Ultimate fallback
        try:
            display.clear(0x0000)
            draw_text_direct(display, "STARTING...", 50, 120, CYAN, 2)
            time.sleep(2)
        except:
            pass

# Show initial menu
if display_available:
    init_menu_display()

# -----------------------
# Main loop
# -----------------------
while True:
    try:
        # -----------------------
        # Read button/joystick inputs
        # -----------------------
        changes = inputs.read_inputs()
        for name, state in changes:
            if state == 0:  # pressed
                print(f"{name} pressed")

                # Send to RPi over USB
                sys.stdout.buffer.write(f"BTN:{name}:PRESSED\n".encode())

                # Handle local menu navigation
                if name == 'down':
                    if selection_index == -1:
                        selection_index = 0  # Start at first item
                    else:
                        selection_index = (selection_index + 1) % len(menu_items)
                    update_menu_display()
                    print(f"DOWN - new selection: {selection_index}")

                elif name == 'up':
                    if selection_index == -1:
                        selection_index = len(menu_items) - 1  # Start at last item
                    else:
                        selection_index = (selection_index - 1) % len(menu_items)
                    update_menu_display()
                    print(f"UP - new selection: {selection_index}")

                elif name == 'press':
                    if selection_index >= 0:  # Only execute if valid selection
                        print(f"SELECTED: {menu_items[selection_index]}")

                        if menu_items[selection_index] == "Go Fly":
                            print("Launching Go Fly menu...")
                            if display_available:
                                display_go_fly_menu(display, inputs)
                                init_menu_display()  # Return to menu

                        elif menu_items[selection_index] == "Bluetooth":
                            print("Launching Bluetooth menu...")
                            if display_available:
                                display_bluetooth_menu(display, inputs)
                                init_menu_display()  # Return to menu
                    else:
                        print("No valid selection - ignoring PRESS")

            else:  # released
                print(f"{name} released")
                # Send to RPi over USB
                sys.stdout.buffer.write(f"BTN:{name}:RELEASED\n".encode())

        # Short delay
        time.sleep_ms(10)

    except Exception as e:
        print(f"Main loop error: {e}")
        time.sleep_ms(100)


