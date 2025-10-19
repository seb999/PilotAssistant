from PIL import Image, ImageDraw, ImageFont
import time
import library.state

def display_fly_page(lcd, font4):
    """Display the Go FLY page with prominent flight-ready interface"""

    # Import Pico2 button states from library module
    try:
        import library.pico2_state
        pico2_button_states = library.pico2_state.pico2_button_states
    except:
        # Fallback if library module not available
        pico2_button_states = {
            'up_pressed': False,
            'down_pressed': False,
            'press_pressed': False,
            'key1_pressed': False,
            'key2_pressed': False
        }

    # Load larger fonts for prominent display
    try:
        font_huge = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 48)
        font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 32)
        font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    except:
        font_huge = font4
        font_large = font4
        font_medium = font4

    last_press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_pico2_press = pico2_button_states.get('press_pressed', False)

    # Small delay to avoid immediate exit from menu selection button press
    time.sleep(0.2)

    while True:
        # Create a fresh image each loop
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Draw large green button in center
        button_width = 180
        button_height = 120
        button_x = (lcd.width - button_width) // 2
        button_y = (lcd.height - button_height) // 2
        
        # Draw green button background with rounded corners effect
        draw.rectangle((button_x, button_y, button_x + button_width, button_y + button_height), 
                      fill="GREEN", outline="DARKGREEN", width=3)
        
        # Add button highlight for 3D effect
        draw.rectangle((button_x + 2, button_y + 2, button_x + button_width - 2, button_y + 15), 
                      fill="LIGHTGREEN")
        
        # Draw "GO FLY" text centered on button
        button_text = "GO FLY"
        text_bbox = draw.textbbox((0, 0), button_text, font=font_huge)
        text_width = text_bbox[2] - text_bbox[0]
        text_height = text_bbox[3] - text_bbox[1]
        
        text_x = button_x + (button_width - text_width) // 2
        text_y = button_y + (button_height - text_height) // 2
        
        # Draw text shadow for depth
        draw.text((text_x + 2, text_y + 2), button_text, fill="DARKGREEN", font=font_huge)
        # Draw main text
        draw.text((text_x, text_y), button_text, fill="WHITE", font=font_huge)
        
        # Draw flight status indicators at the top
        status_text = "FLIGHT READY"
        status_bbox = draw.textbbox((0, 0), status_text, font=font_medium)
        status_width = status_bbox[2] - status_bbox[0]
        status_x = (lcd.width - status_width) // 2
        draw.text((status_x, 5), status_text, fill="CYAN", font=font_medium)
        
        # Draw simple flight indicators
        # GPS status (simplified - assume ready)
        draw.text((5, lcd.height - 60), "GPS: READY", fill="GREEN", font=font_medium)
        
        # Sensor status (simplified - assume ready)
        draw.text((5, lcd.height - 35), "SENSORS: OK", fill="GREEN", font=font_medium)
        
        # Exit instruction at bottom right
        exit_text = "PRESS TO EXIT"
        exit_bbox = draw.textbbox((0, 0), exit_text, font=font_medium)
        exit_width = exit_bbox[2] - exit_bbox[0]
        draw.text((lcd.width - exit_width - 5, lcd.height - 25), exit_text, fill="WHITE", font=font_medium)
        
        # Add decorative border
        draw.rectangle((2, 2, lcd.width-3, lcd.height-3), outline="MAGENTA", width=2)

        # Handle center press button to exit (LCD or Pico2)
        press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        current_pico2_press = pico2_button_states['press_pressed']

        if (press == 0 and last_press == 1) or (current_pico2_press and not last_pico2_press):
            print("PRESS pressed → returning to main menu")
            return

        last_press = press
        last_pico2_press = current_pico2_press

        # Display everything
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        time.sleep(0.1)