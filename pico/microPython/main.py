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
# Digital buttons only (no joystick pins since we use ADC)
button_pins = {
    'key1': 2,
    'key2': 3,
   # 'key3': 17, button glue by mistake
    'key4': 15
}

# ADC joystick configuration
joystick_config = {
    'vrx': 27,  # X-axis ADC pin
    'vry': 26,  # Y-axis ADC pin
    'sw': 16,   # Button pin
    'center_min': 25000,
    'center_max': 40000
}

inputs = InputHandler(pin_map=button_pins, joystick_config=joystick_config)
print("InputHandler initialized")

# -----------------------
# Initialize Display
# -----------------------
try:
    # Initialize display
    print("Initializing display...")
    import gc
    gc.collect()
    print(f"Free memory before display init: {gc.mem_free()} bytes")

    display = ST7789()  # Use default 320x240 settings
    print("Display initialized successfully")
    print(f"Free memory after display init: {gc.mem_free()} bytes")
    display_available = True
except Exception as e:
    print(f"Display not available: {e}")
    import sys
    sys.print_exception(e)
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
menu_items = ['Go Fly', 'Bluetooth', 'Gyro Offset']
selection_index = -1  # No default selection
last_selection_index = -2  # Track previous selection for partial updates

# Colors
BLACK = rgb565(0, 0, 0)
WHITE = rgb565(255, 255, 255)
CYAN = rgb565(0, 255, 255)
YELLOW = rgb565(255, 255, 0)
MAGENTA = rgb565(255, 0, 255)
PURPLE = rgb565(128, 0, 128)

print(f"Color values - BLACK: 0x{BLACK:04X}, CYAN: 0x{CYAN:04X}, MAGENTA: 0x{MAGENTA:04X}")

# Menu item positions - no title, larger text (updated for 320x240)
label_positions = [
    (5, 20, 315, 55),   # Go Fly - wider for 320px screen
    (5, 60, 315, 95),   # Bluetooth - wider for 320px screen
    (5, 100, 315, 135)  # Gyro Offset - wider for 320px screen
]

def draw_icon(display, icon_name, x, y, color):
    """Draw a simple icon at position x, y"""
    if icon_name == "Go Fly":
        # Aircraft icon - simple plane shape
        # Wings (horizontal line)
        display.draw_rect(x, y + 10, 24, 3, color)
        # Fuselage (vertical line)
        display.draw_rect(x + 10, y, 4, 20, color)
        # Tail (small horizontal)
        display.draw_rect(x + 6, y, 12, 3, color)

    elif icon_name == "Bluetooth":
        # Bluetooth icon - stylized B
        # Vertical line
        display.draw_rect(x + 8, y, 3, 20, color)
        # Top triangle
        for i in range(10):
            display.draw_pixel(x + 11 + i//2, y + i, color)
        # Bottom triangle
        for i in range(10):
            display.draw_pixel(x + 11 + i//2, y + 19 - i, color)

    elif icon_name == "Gyro Offset":
        # Gyro/calibration icon - circular with center dot
        # Outer circle (draw as octagon for simplicity)
        display.draw_rect(x + 4, y, 12, 2, color)      # Top
        display.draw_rect(x + 4, y + 18, 12, 2, color)  # Bottom
        display.draw_rect(x, y + 4, 2, 12, color)      # Left
        display.draw_rect(x + 18, y + 4, 2, 12, color)  # Right
        display.draw_rect(x + 2, y + 2, 2, 2, color)   # Corners
        display.draw_rect(x + 16, y + 2, 2, 2, color)
        display.draw_rect(x + 2, y + 16, 2, 2, color)
        display.draw_rect(x + 16, y + 16, 2, 2, color)
        # Center crosshair
        display.draw_rect(x + 7, y + 9, 6, 2, color)  # Horizontal
        display.draw_rect(x + 9, y + 7, 2, 6, color)  # Vertical

def draw_menu_item(item_index, selected=False):
    """Draw a single menu item efficiently with icon"""
    if not display_available or item_index >= len(menu_items):
        return

    rect = label_positions[item_index]
    item = menu_items[item_index]
    menu_font_size = 3

    if selected:
        # Selected item - draw YELLOW rectangle background
        rect_width = rect[2] - rect[0]
        rect_height = rect[3] - rect[1]
        display.draw_rect(rect[0], rect[1], rect_width, rect_height, YELLOW)

        # Draw icon on left side
        icon_x = rect[0] + 8
        icon_y = rect[1] + (rect_height - 20) // 2
        draw_icon(display, item, icon_x, icon_y, BLACK)

        # Draw BLACK text on YELLOW background with vertical centering (shifted right for icon)
        text_height = 8 * menu_font_size
        text_y = rect[1] + (rect_height - text_height) // 2 - 2
        draw_text_direct(display, item, rect[0] + 40, text_y, BLACK, menu_font_size)
    else:
        # Unselected item - black background and draw YELLOW text
        rect_width = rect[2] - rect[0]
        rect_height = rect[3] - rect[1]
        display.draw_rect(rect[0], rect[1], rect_width, rect_height, BLACK)  # Keep black background

        # Draw icon on left side
        icon_x = rect[0] + 8
        icon_y = rect[1] + (rect_height - 20) // 2
        draw_icon(display, item, icon_x, icon_y, YELLOW)

        text_y = rect[1] + 5  # Small offset from top
        draw_text_direct(display, item, rect[0] + 40, text_y, YELLOW, menu_font_size)

def init_menu_display():
    """Initialize the menu display (full redraw)"""
    global last_selection_index
    if not display_available:
        return

    # Clear screen with black background
    display.clear(BLACK)

    # No title - just clear screen background

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

# Show splash screen for 2 seconds IMMEDIATELY
if display_available:
    # First clear the display immediately to avoid showing anything else
    display.clear(BLACK)

    try:
        # Use enhanced splash loader with multiple fallback options
        show_splash_screen(display)

        print("Splash screen with overlay displayed")

        # Wait for 2 seconds
        time.sleep(2)

        # Now show the menu after splash is done
        init_menu_display()

    except Exception as e:
        print(f"Error displaying splash: {e}")
        # Ultimate fallback
        try:
            display.clear(BLACK)
            draw_text_direct(display, "STARTING...", 80, 120, YELLOW, 2)
            time.sleep(2)
            # Show menu after fallback splash
            init_menu_display()
        except:
            # If everything fails, still try to show menu
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

                elif name == 'right':
                    if selection_index >= 0:  # Only execute if valid selection
                        print(f"SELECTED: {menu_items[selection_index]}")

                        if menu_items[selection_index] == "Go Fly":
                            print("Launching Go Fly menu...")
                            if display_available:
                                display_go_fly_menu(display, inputs)
                                # Clear screen and redraw menu after exiting
                                display.clear(BLACK)
                                init_menu_display()

                        elif menu_items[selection_index] == "Bluetooth":
                            print("Launching Bluetooth menu...")
                            if display_available:
                                display_bluetooth_menu(display, inputs)
                                # Clear screen and redraw menu after exiting
                                display.clear(BLACK)
                                init_menu_display()

                        elif menu_items[selection_index] == "Gyro Offset":
                            print("Launching Gyro Offset menu...")
                            if display_available:
                                # TODO: Create gyro_offset_menu.py
                                display.clear(BLACK)
                                draw_text_direct(display, "GYRO OFFSET", 60, 100, YELLOW, 2)
                                draw_text_direct(display, "Coming Soon", 70, 130, WHITE, 2)
                                time.sleep(2)
                                # Clear screen and redraw menu after exiting
                                display.clear(BLACK)
                                init_menu_display()
                    else:
                        print("No valid selection - ignoring RIGHT")

            else:  # released
                print(f"{name} released")
                # Send to RPi over USB
                sys.stdout.buffer.write(f"BTN:{name}:RELEASED\n".encode())

        # Short delay
        time.sleep_ms(10)

    except Exception as e:
        print(f"Main loop error: {e}")
        time.sleep_ms(100)


