"""
Go Fly Menu for Raspberry Pi Pico2
Blank page with title - placeholder for future implementation
"""

import time


def rgb565(r, g, b):
    """Convert RGB888 to RGB565 color format"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def create_image_buffer(width, height, bg_color):
    """Create a simple image buffer filled with background color"""
    color_bytes = bg_color.to_bytes(2, 'big')
    return color_bytes * (width * height)


def display_go_fly_menu(lcd, input_handler):
    """Display Go Fly menu page - currently blank with title only"""
    print("Displaying Go Fly menu...")

    # Colors
    BLACK = rgb565(0, 0, 0)
    MAGENTA = rgb565(255, 0, 255)
    CYAN = rgb565(0, 255, 255)

    def update_display():
        """Update the go fly menu display"""
        # Create magenta background to show Go Fly menu is active
        buffer = create_image_buffer(lcd.width, lcd.height, MAGENTA)
        lcd.show_image(buffer)

    # Show initial display
    update_display()

    # Main menu loop
    while True:
        # Read input changes
        changes = input_handler.read_inputs()

        for button, state in changes:
            # Only process button press (state == 0)
            if state == 0:
                print(f"Go Fly menu: {button} button pressed")

                # KEY1 or any button to go back (simplified for now)
                if button in ['key1', 'press']:
                    print("Returning to main menu...")
                    return

        time.sleep(0.05)  # Small delay for responsiveness