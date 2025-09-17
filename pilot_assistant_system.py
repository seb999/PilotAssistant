# Import existing libraries and all classes from the classes folder
import time
from PIL import Image, ImageDraw, ImageFont
from library import ST7789
from menu2.bluetooth_menu import display_bluetooth_menu
from menu2.go_fly_menu import display_go_fly_menu


def main():
    """Main menu system for Pilot Assistant"""
    # LCD setup
    lcd = ST7789.ST7789()
    lcd.Init()
    lcd.clear()
    lcd.bl_DutyCycle(50)

    # Fonts
    font4 = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 35)
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 30)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 20)
    font_title = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 28)

    # Splash screen
    try:
        image = Image.open('./images/output.png')
        im_r = image.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)
        time.sleep(2)
    except:
        # If splash image doesn't exist, show text splash
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        draw.text((20, 100), "PILOT ASSISTANT", fill="CYAN", font=font_large)
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)
        time.sleep(2)

    # Menu setup
    menu_items = ['Go Fly', 'Bluetooth']
    selection_index = -1  # No default selection

    # Initialize button states by reading current state
    last_down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
    last_up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    last_press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)

    label_positions = [
        (5, 65, 235, 95),
        (5, 100, 235, 130)
    ]

    def update_menu_display(index):
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Title with ribbon background
        title_text = "PILOT ASSISTANT"
        title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
        title_width = title_bbox[2] - title_bbox[0]
        title_height = title_bbox[3] - title_bbox[1]

        # Draw ribbon background (full width)
        draw.rectangle((0, 5, lcd.width, title_height + 15), fill="BLACK")

        # Center the title text on the ribbon
        title_x = (lcd.width - title_width) // 2
        draw.text((title_x, 8), title_text, fill="MAGENTA", font=font_title)

        for i, label in enumerate(menu_items):
            if i == index and index >= 0:  # Only highlight if valid selection
                draw.rectangle(label_positions[i], fill="CYAN")
                # Position the text with slight offset from top (not perfectly centered)
                text_bbox = draw.textbbox((0, 0), label, font=font4)
                text_height = text_bbox[3] - text_bbox[1]
                rect_height = label_positions[i][3] - label_positions[i][1]
                text_y = label_positions[i][1] + (rect_height - text_height) // 2 - 2
                draw.text((label_positions[i][0], text_y), label, fill="BLACK", font=font4)
            else:
                draw.text((label_positions[i][0], label_positions[i][1]), label, fill="CYAN", font=font4)

        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

    # Show initial menu
    update_menu_display(selection_index)
    print(f"Initial menu displayed with selection_index: {selection_index}")
    print(f"Initial button states - DOWN: {last_down_state}, UP: {last_up_state}, PRESS: {last_press_state}")

    # Main menu loop
    try:
        while True:
            down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
            up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
            press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)

            # Down button press
            if down_state == 0 and last_down_state == 1:
                print(f"DOWN pressed - current selection: {selection_index}")
                if selection_index == -1:
                    selection_index = 0  # Start at first item
                else:
                    selection_index = (selection_index + 1) % len(menu_items)
                update_menu_display(selection_index)
                print(f"DOWN - new selection: {selection_index}")

            # Up button press
            if up_state == 0 and last_up_state == 1:
                print(f"UP pressed - current selection: {selection_index}")
                if selection_index == -1:
                    selection_index = len(menu_items) - 1  # Start at last item
                else:
                    selection_index = (selection_index - 1) % len(menu_items)
                update_menu_display(selection_index)
                print(f"UP - new selection: {selection_index}")

            # Center press button (SELECT)
            if press_state == 0 and last_press_state == 1:
                print(f"PRESS button pressed - selection_index: {selection_index}")
                if selection_index >= 0:  # Only execute if valid selection
                    print(f"SELECTED: {menu_items[selection_index]}")

                    if menu_items[selection_index] == "Go Fly":
                        print("Launching Go Fly menu...")
                        display_go_fly_menu(lcd)
                        update_menu_display(selection_index)  # Return to menu

                    elif menu_items[selection_index] == "Bluetooth":
                        print("Launching Bluetooth menu...")
                        display_bluetooth_menu(lcd)
                        update_menu_display(selection_index)  # Return to menu
                else:
                    print("No valid selection - ignoring PRESS")

            # Update button states
            last_down_state = down_state
            last_up_state = up_state
            last_press_state = press_state

            time.sleep(0.1)

    except KeyboardInterrupt:
        print("Shutting down...")
    finally:
        lcd.clear()


if __name__ == "__main__":
    main()