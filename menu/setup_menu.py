from PIL import Image, ImageDraw
import time
import library.state

def display_setup_page(lcd, font4):
    pitch = 0
    bank = 0
    last_up = 1
    last_down = 1
    last_left = 1
    last_right = 1
    last_center = 0
    library.state.pitch_offset = pitch
    library.state.bank_offset = bank

    while True:
        # Create a fresh image each loop
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((5, 5), 'SETUP', fill="MAGENTA", font=font4)

        # Title
        draw.text((10, 50), 'Pitch:', fill="CYAN", font=font4)
        draw.text((140, 50), str(pitch), fill="LIME", font=font4)
        draw.text((10, 90), 'Bank:', fill="CYAN", font=font4)
        draw.text((140, 90), str(bank), fill="LIME", font=font4)

        # Joystick visual center and size
        cx, cy = 120, 170
        size = 12
        center_size = 15  # Half-size of square

        # UP
        up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        if up == 0:
            draw.polygon([(cx - size, cy - 2*size), (cx, cy - 3*size), (cx + size, cy - 2*size)], outline=255, fill=0x00ff00)
            if last_up == 1:
                pitch -= 1
                library.state.pitch_offset -= 1
                print("UP pressed → pitch =", pitch)
        else:
            draw.polygon([(cx - size, cy - 2*size), (cx, cy - 3*size), (cx + size, cy - 2*size)], outline=255, fill=0)
        last_up = up

        # DOWN
        down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
        if down == 0:
            draw.polygon([(cx - size, cy + 2*size), (cx, cy + 3*size), (cx + size, cy + 2*size)], outline=255, fill=0x00ff00)
            if last_down == 1:
                pitch += 1
                library.state.pitch_offset += 1
                print("DOWN pressed → pitch =", pitch)
        else:
            draw.polygon([(cx - size, cy + 2*size), (cx, cy + 3*size), (cx + size, cy + 2*size)], outline=255, fill=0)
        last_down = down

        # LEFT
        left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
        if left == 0:
            draw.polygon([(cx - 2*size, cy - size), (cx - 3*size, cy), (cx - 2*size, cy + size)], outline=255, fill=0x00ff00)
            if last_left == 1:
                bank -= 1
                library.state.bank_offset -= 1
                print("LEFT pressed → bank =", bank)
        else:
            draw.polygon([(cx - 2*size, cy - size), (cx - 3*size, cy), (cx - 2*size, cy + size)], outline=255, fill=0)
        last_left = left

        # RIGHT
        right = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)
        if right == 0:
            draw.polygon([(cx + 2*size, cy - size), (cx + 3*size, cy), (cx + 2*size, cy + size)], outline=255, fill=0x00ff00)
            if last_right == 1:
                bank += 1
                library.state.bank_offset += 1
                print("RIGHT pressed → bank =", bank)
        else:
            draw.polygon([(cx + 2*size, cy - size), (cx + 3*size, cy), (cx + 2*size, cy + size)], outline=255, fill=0)
        last_right = right
       
        center = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        if center == 0:
            draw.rectangle((cx - center_size, cy - center_size, cx + center_size, cy + center_size),
                        outline=255, fill=0x00ff00)
            if last_center == 1:
                print("Center pressed → exiting")
                break  # exit on falling edge (just pressed)
        else:
            draw.rectangle((cx - center_size, cy - center_size, cx + center_size, cy + center_size),
                        outline=255, fill=0)
        last_center = center

        # Display everything
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        time.sleep(0.1)