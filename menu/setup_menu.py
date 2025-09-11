from PIL import Image, ImageDraw, ImageFont
import time
import library.state

def display_setup_page(lcd, font4):
    # Define better fonts for this menu
    font_title = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 32)
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 30)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    
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

        # Title with cyan background
        title_text = 'SETUP'
        title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
        title_width = title_bbox[2] - title_bbox[0]
        title_height = title_bbox[3] - title_bbox[1]
        
        # Draw cyan background for title
        draw.rectangle((0, 5, lcd.width, title_height + 15), fill="CYAN")
        draw.text((10, 8), title_text, fill="BLACK", font=font_title)

        # Display pitch and bank with larger fonts
        draw.text((10, 50), 'PITCH:', fill="CYAN", font=font_large)
        draw.text((120, 50), str(pitch), fill="LIME", font=font_large)
        draw.text((10, 80), 'BANK:', fill="CYAN", font=font_large)
        draw.text((120, 80), str(bank), fill="LIME", font=font_large)

        # Joystick visual - traditional layout with all arrows around center
        joy_cx, joy_cy = 180, 70  # Center position for joystick
        arrow_size = 12
        spacing = 6  # Additional spacing between arrows

        # UP (pitch control) - small arrow next to pitch value
        up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        if up == 0:
            draw.polygon([(joy_cx - arrow_size, joy_cy - arrow_size - spacing), (joy_cx, joy_cy - arrow_size*2 - spacing), (joy_cx + arrow_size, joy_cy - arrow_size - spacing)], outline="CYAN", fill="LIME")
            if last_up == 1:
                pitch -= 1
                library.state.pitch_offset -= 1
                print("UP pressed → pitch =", pitch)
        else:
            draw.polygon([(joy_cx - arrow_size, joy_cy - arrow_size - spacing), (joy_cx, joy_cy - arrow_size*2 - spacing), (joy_cx + arrow_size, joy_cy - arrow_size - spacing)], outline="CYAN", fill=0)
        last_up = up

        # DOWN arrow (pitch control) - bottom of joystick
        down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
        if down == 0:
            draw.polygon([(joy_cx - arrow_size, joy_cy + arrow_size + spacing), (joy_cx, joy_cy + arrow_size*2 + spacing), (joy_cx + arrow_size, joy_cy + arrow_size + spacing)], outline="CYAN", fill="LIME")
            if last_down == 1:
                pitch += 1
                library.state.pitch_offset += 1
                print("DOWN pressed → pitch =", pitch)
        else:
            draw.polygon([(joy_cx - arrow_size, joy_cy + arrow_size + spacing), (joy_cx, joy_cy + arrow_size*2 + spacing), (joy_cx + arrow_size, joy_cy + arrow_size + spacing)], outline="CYAN", fill=0)
        last_down = down

        # LEFT arrow (bank control) - left side of joystick
        left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
        if left == 0:
            draw.polygon([(joy_cx - arrow_size - spacing, joy_cy - arrow_size), (joy_cx - arrow_size*2 - spacing, joy_cy), (joy_cx - arrow_size - spacing, joy_cy + arrow_size)], outline="CYAN", fill="LIME")
            if last_left == 1:
                bank -= 1
                library.state.bank_offset -= 1
                print("LEFT pressed → bank =", bank)
        else:
            draw.polygon([(joy_cx - arrow_size - spacing, joy_cy - arrow_size), (joy_cx - arrow_size*2 - spacing, joy_cy), (joy_cx - arrow_size - spacing, joy_cy + arrow_size)], outline="CYAN", fill=0)
        last_left = left

        # RIGHT arrow (bank control) - right side of joystick
        right = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)
        if right == 0:
            draw.polygon([(joy_cx + arrow_size + spacing, joy_cy - arrow_size), (joy_cx + arrow_size*2 + spacing, joy_cy), (joy_cx + arrow_size + spacing, joy_cy + arrow_size)], outline="CYAN", fill="LIME")
            if last_right == 1:
                bank += 1
                library.state.bank_offset += 1
                print("RIGHT pressed → bank =", bank)
        else:
            draw.polygon([(joy_cx + arrow_size + spacing, joy_cy - arrow_size), (joy_cx + arrow_size*2 + spacing, joy_cy), (joy_cx + arrow_size + spacing, joy_cy + arrow_size)], outline="CYAN", fill=0)
        last_right = right
       
        # # Center circle of joystick
        # center = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        # if center == 0:
        #     draw.ellipse((joy_cx - 8, joy_cy - 8, joy_cx + 8, joy_cy + 8), outline="CYAN", fill="LIME")
        #     if last_center == 1:
        #         print("Center pressed → exiting")
        #         break  # exit on falling edge (just pressed)
        # else:
        #     draw.ellipse((joy_cx - 8, joy_cy - 8, joy_cx + 8, joy_cy + 8), outline="CYAN", fill=0)
        # last_center = center
        

        # Display everything
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        time.sleep(0.1)