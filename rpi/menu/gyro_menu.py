from PIL import Image, ImageDraw, ImageFont
import time
import math
import smbus2
import library.state

# ADXL345 constants
ADXL345_ADDRESS = 0x53
ADXL345_POWER_CTL = 0x2D
ADXL345_DATA_FORMAT = 0x31
ADXL345_DATAX0 = 0x32

def init_adxl345():
    """Initialize ADXL345 accelerometer"""
    try:
        bus = smbus2.SMBus(1)
        # Wake up the ADXL345
        bus.write_byte_data(ADXL345_ADDRESS, ADXL345_POWER_CTL, 0x08)
        # Set data format (±2g, full resolution)
        bus.write_byte_data(ADXL345_ADDRESS, ADXL345_DATA_FORMAT, 0x08)
        return bus
    except:
        return None

def read_adxl345_data(bus):
    """Read accelerometer data and calculate pitch/roll angles"""
    if not bus:
        return 0, 0
    
    try:
        # Read 6 bytes from DATAX0 to DATAZ1
        data = bus.read_i2c_block_data(ADXL345_ADDRESS, ADXL345_DATAX0, 6)
        
        # Convert to signed 16-bit integers
        x = int.from_bytes(data[0:2], byteorder='little', signed=True) / 256.0
        y = int.from_bytes(data[2:4], byteorder='little', signed=True) / 256.0
        z = int.from_bytes(data[4:6], byteorder='little', signed=True) / 256.0
        
        # Calculate pitch and roll in degrees 
        pitch = math.degrees(math.atan2(-y, z))  # Pitch uses only Y and Z axes
        roll = math.degrees(math.atan2(x, z))   # Roll uses only X and Z axes
        
        return pitch, roll
    except:
        return 0, 0
    
def draw_artificial_horizon(draw, width, height, pitch, roll):
    """Draw artificial horizon with pitch and roll indication using full screen"""

    # Parameters
    pitch_scale = 3  # pixels per degree
    roll_rad = math.radians(roll)

    # Calculate pitch offset (move horizon line up/down)
    pitch_offset = pitch * pitch_scale

    # Create horizon line across full width
    line_length = width
    x1_local = -line_length
    y1_local = pitch_offset
    x2_local = line_length
    y2_local = pitch_offset

    # Rotate horizon line by roll
    cos_r = math.cos(roll_rad)
    sin_r = math.sin(roll_rad)

    def rotate(x, y):
        return (
            x * cos_r - y * sin_r,
            x * sin_r + y * cos_r
        )

    x1_rot, y1_rot = rotate(x1_local, y1_local)
    x2_rot, y2_rot = rotate(x2_local, y2_local)

    # Center on screen
    cx = width // 2
    cy = height // 2
    x1_screen = x1_rot + cx
    y1_screen = y1_rot + cy
    x2_screen = x2_rot + cx
    y2_screen = y2_rot + cy

    # Draw sky background
    draw.rectangle((0, 0, width, height), fill="#4A90E2")

    # Draw ground polygon below the horizon
    ground_poly = [
        (0, height),
        (width, height),
        (x2_screen, y2_screen),
        (x1_screen, y1_screen)
    ]
    draw.polygon(ground_poly, fill="#8B4513")

    # Draw horizon line
    draw.line([(x1_screen, y1_screen), (x2_screen, y2_screen)], fill="WHITE", width=3)

    # Draw pitch ladder marks
    for pitch_mark in [-30, -20, -10, 10, 20, 30]:
        offset = - (pitch_mark - pitch) * pitch_scale
        mark_length = 30 if pitch_mark % 20 == 0 else 20
        x1, y1 = rotate(-mark_length, offset)
        x2, y2 = rotate(mark_length, offset)
        x1 += cx
        y1 += cy
        x2 += cx
        y2 += cy

        # Only draw if within screen bounds
        if 0 <= y1 <= height and 0 <= y2 <= height:
            draw.line([(x1, y1), (x2, y2)], fill="WHITE", width=2)

    # Draw roll indicator triangle at top
    triangle_size = 6
    top_x = cx + (height // 2 - 20) * math.sin(roll_rad)
    top_y = 20 - (height // 2 - 20) * math.cos(roll_rad)
    draw.polygon([
        (top_x, top_y - triangle_size),
        (top_x - triangle_size, top_y + triangle_size),
        (top_x + triangle_size, top_y + triangle_size)
    ], fill="YELLOW", outline="CYAN")

    # Draw aircraft symbol in center
    draw.line([(cx - 20, cy), (cx - 5, cy)], fill="YELLOW", width=3)
    draw.line([(cx + 5, cy), (cx + 20, cy)], fill="YELLOW", width=3)
    draw.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill="YELLOW")

def display_setup_page(lcd, font4):
    # Import Pico2 button states from library module
    try:
        import library.pico_state
        pico2_button_states = library.pico_state.pico2_button_states
    except:
        # Fallback if library module not available
        pico2_button_states = {
            'up_pressed': False,
            'down_pressed': False,
            'press_pressed': False,
            'key1_pressed': False,
            'key2_pressed': False
        }
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 32)
    
    pitch = 0
    bank = 0
    last_up = 1
    last_down = 1
    last_left = 1
    last_right = 1
    last_press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_key2 = lcd.digital_read(lcd.GPIO_KEY2_PIN)

    # Pico2 button state tracking for edge detection
    # Initialize with current state to avoid immediate trigger from menu selection
    last_pico2_states = pico2_button_states.copy()
    # Ensure all expected keys exist in last_pico2_states
    for key in ['up_pressed', 'down_pressed', 'left_pressed', 'right_pressed', 'press_pressed', 'key1_pressed', 'key2_pressed']:
        if key not in last_pico2_states:
            last_pico2_states[key] = False

    library.state.pitch_offset = pitch
    library.state.bank_offset = bank

    # Initialize ADXL345
    adxl_bus = init_adxl345()

    # Small delay to avoid immediate exit from menu selection button press
    time.sleep(0.2)

    while True:
        # Create a fresh image each loop
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Handle joystick input (LCD or Pico2)
        up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        pico2_up_pressed = pico2_button_states['up_pressed'] and not last_pico2_states['up_pressed']
        if (up == 0 and last_up == 1) or pico2_up_pressed:
            pitch -= 1
            library.state.pitch_offset -= 1
            print("UP pressed → pitch =", pitch)
        last_up = up

        down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
        pico2_down_pressed = pico2_button_states['down_pressed'] and not last_pico2_states['down_pressed']
        if (down == 0 and last_down == 1) or pico2_down_pressed:
            pitch += 1
            library.state.pitch_offset += 1
            print("DOWN pressed → pitch =", pitch)
        last_down = down

        left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
        pico2_left_pressed = pico2_button_states.get('left_pressed', False) and not last_pico2_states.get('left_pressed', False)
        if (left == 0 and last_left == 1) or pico2_left_pressed:
            bank -= 1
            library.state.bank_offset -= 1
            print("LEFT pressed → bank =", bank)
        last_left = left

        right = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)
        pico2_right_pressed = pico2_button_states.get('right_pressed', False) and not last_pico2_states.get('right_pressed', False)
        if (right == 0 and last_right == 1) or pico2_right_pressed:
            bank += 1
            library.state.bank_offset += 1
            print("RIGHT pressed → bank =", bank)
        last_right = right

        press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        pico2_press_pressed = pico2_button_states['press_pressed'] and not last_pico2_states['press_pressed']

        if (press == 0 and last_press == 1) or pico2_press_pressed:
            print("PRESS pressed → returning to main menu")
            return
        last_press = press

        # Handle KEY2 press to switch to streaming page (LCD or Pico2)
        key2 = lcd.digital_read(lcd.GPIO_KEY2_PIN)
        pico2_key2_pressed = pico2_button_states['key2_pressed'] and not last_pico2_states['key2_pressed']
        if (key2 == 0 and last_key2 == 1) or pico2_key2_pressed:
            print("KEY2 pressed → switching to streaming page")
            # Dynamic import to avoid circular dependency
            from menu.stream_menu import display_stream_page
            display_stream_page(lcd)
            # Re-initialize ADXL345 after returning from stream page
            adxl_bus = init_adxl345()
        last_key2 = key2

        # Update Pico2 button states for edge detection
        last_pico2_states = pico2_button_states.copy()
       
        accel_pitch, accel_roll = read_adxl345_data(adxl_bus)
        
        # Apply calibration offsets
        calibrated_pitch = accel_pitch - library.state.pitch_offset
        calibrated_roll = accel_roll - library.state.bank_offset
        
        # Draw artificial horizon using full screen
        draw_artificial_horizon(draw, lcd.width, lcd.height, calibrated_pitch, calibrated_roll)
        
        # Display pitch and roll labels overlaid on horizon
        draw.text((5, 5), 'Pitch', fill="YELLOW", font=font_large)
        draw.text((5, 40), str(pitch), fill="YELLOW", font=font_large)
        
        # Right-align Roll label and value
        roll_text = 'Roll'
        roll_value = str(bank)
        roll_bbox = draw.textbbox((0, 0), roll_text, font=font_large)
        roll_width = roll_bbox[2] - roll_bbox[0]
        value_bbox = draw.textbbox((0, 0), roll_value, font=font_large)
        value_width = value_bbox[2] - value_bbox[0]
        
        draw.text((lcd.width - roll_width - 5, 5), roll_text, fill="YELLOW", font=font_large)
        draw.text((lcd.width - value_width - 5, 40), roll_value, fill="YELLOW", font=font_large)

        # Display everything
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        time.sleep(0.1)