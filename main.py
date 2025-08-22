import spidev as SPI
import logging
from library import ST7789
import time
from PIL import Image, ImageDraw, ImageFont
from menu.setup_menu import display_setup_page
from menu.stream_menu import display_stream_page

logging.basicConfig(level=logging.DEBUG)

# LCD setup
lcd = ST7789.ST7789()
lcd.Init()
lcd.clear()
lcd.bl_DutyCycle(50)

# Fonts
font4 = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 35)

# Splash screen
image = Image.open('./images/output.png')
im_r = image.rotate(270)
lcd.ShowImage(im_r)
time.sleep(2)

# Menu setup
menu_items = ['SETUP', 'ACTIVATE', 'STATUS', 'SHUTDOWN']
selection_index = -1
last_down_state = -1
last_up_state = -1
last_press_state = -1
label_positions = [
    (5, 5, 235, 35),
    (5, 40, 235, 70),
    (5, 75, 235, 105),
    (5, 110, 235, 140)
]

def update_menu_display(index):
    background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
    draw = ImageDraw.Draw(background)

    for i, label in enumerate(menu_items):
        if i == index:
            draw.rectangle(label_positions[i], fill="CYAN")
            draw.text((label_positions[i][0], label_positions[i][1]), label, fill="BLACK", font=font4)
        else:
            draw.text((label_positions[i][0], label_positions[i][1]), label, fill="CYAN", font=font4)

    im_r = background.rotate(270)
    lcd.ShowImage(im_r)

# Show initial menu
update_menu_display(selection_index)

while True:
    down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
    up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)

    # Down button press
    if down_state == 0 and last_down_state == 1:
        selection_index = (selection_index + 1) % len(menu_items)
        update_menu_display(selection_index)
        print("Joystick DOWN")

    # Up button press
    if up_state == 0 and last_up_state == 1:
        selection_index = (selection_index - 1) % len(menu_items)
        update_menu_display(selection_index)
        print("Joystick UP")

    # Center press button (SELECT)
    if press_state == 0 and last_press_state == 1:
        print(f"SELECTED: {menu_items[selection_index]}")
        if menu_items[selection_index] == "SETUP":
            display_setup_page(lcd, font4)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "ACTIVATE":
            display_stream_page(lcd)
            update_menu_display(selection_index)  # Return to menu

    last_down_state = down_state
    last_up_state = up_state
    last_press_state = press_state
    time.sleep(0.1)