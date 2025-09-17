import spidev as SPI
import logging
from library import ST7789
import time
import subprocess
import serial
import smbus2
import threading
from PIL import Image, ImageDraw, ImageFont
from menu.gyro_menu import display_setup_page
from menu.stream_menu import display_stream_page
from menu.gps_menu import display_gps_page
from menu.traffic_menu import display_traffic_page
from menu.fly_menu import display_fly_page
from menu.bluetooth_menu import display_bluetooth_page
from picamera2 import Picamera2
from gpiozero import DigitalOutputDevice
from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT
import library.pico_state

logging.basicConfig(level=logging.DEBUG)

# LCD setup
lcd = ST7789.ST7789()
lcd.Init()
lcd.clear()
lcd.bl_DutyCycle(50)

# Pico2 configuration
PICO2_PORT = "/dev/ttyACM0"
PICO2_BAUDRATE = 115200

# Shared button state variables for Pico2
pico2_button_states = {
    'up_pressed': False,
    'down_pressed': False,
    'left_pressed': False,
    'right_pressed': False,
    'press_pressed': False,
    'key1_pressed': False,
    'key2_pressed': False
}

# Make button states available to menus via library module
library.pico_state.pico2_button_states = pico2_button_states

# Fonts
font4 = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 35)
font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 30)
font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 20)


def pico2_listener():
    """Background thread to listen for Pico2 button presses"""
    global pico2_button_states

    try:
        ser = serial.Serial(PICO2_PORT, PICO2_BAUDRATE, timeout=1)
        print(f"Pico2 listener started on {PICO2_PORT}")

        while True:
            try:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line:
                    print(f"Pico2 received: {line}")

                    # Handle both formats: "BTN:name:PRESSED" and "name pressed"
                    if line.startswith("BTN:"):
                        # Format: BTN:name:PRESSED / BTN:name:RELEASED
                        parts = line.split(":")
                        if len(parts) == 3:
                            _, name, state = parts
                            is_pressed = state == "PRESSED"
                    else:
                        # Format: "name pressed" / "name released"
                        parts = line.split(" ")
                        if len(parts) == 2:
                            name, state = parts
                            is_pressed = state == "pressed"
                        else:
                            continue

                    # Map Pico2 button names to our state variables
                    if name.lower() == "up":
                        pico2_button_states['up_pressed'] = is_pressed
                    elif name.lower() == "down":
                        pico2_button_states['down_pressed'] = is_pressed
                    elif name.lower() == "left":
                        pico2_button_states['left_pressed'] = is_pressed
                    elif name.lower() == "right":
                        pico2_button_states['right_pressed'] = is_pressed
                    elif name.lower() in ["press", "center", "select"]:
                        pico2_button_states['press_pressed'] = is_pressed
                    elif name.lower() == "key1":
                        pico2_button_states['key1_pressed'] = is_pressed
                    elif name.lower() == "key2":
                        pico2_button_states['key2_pressed'] = is_pressed

                    print(f"Pico2 Button {name} {state} -> {is_pressed}")

            except UnicodeDecodeError:
                continue
            except Exception as e:
                print(f"Error reading from Pico2: {e}")
                time.sleep(1)

    except Exception as e:
        print(f"Error connecting to Pico2: {e}")
    finally:
        try:
            ser.close()
        except:
            pass

# Start Pico2 listener thread
pico2_thread = threading.Thread(target=pico2_listener, daemon=True)
pico2_thread.start()

def check_wifi_status():
    """Check WiFi connection status"""
    try:
        # Check if we have an IP address (indicating network connection)
        result = subprocess.run(['hostname', '-I'], capture_output=True, text=True, timeout=1)
        if result.returncode == 0 and result.stdout.strip():
            return "OK"
        else:
            return "FAIL"
    except:
        return "FAIL"

def check_gps_status():
    """Check GPS module status - only OK if we get valid coordinates"""
    try:
        # Try to open GPS port with short timeout
        ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=0.5)
        
        # Read only a few lines quickly to avoid blocking
        for _ in range(5):  # Try reading up to 5 lines only
            try:
                if ser.in_waiting > 0:  # Only read if data is available
                    line = ser.readline().decode('ascii', errors='replace').strip()
                    if line.startswith('$GPGGA'):
                        parts = line.split(',')
                        if len(parts) >= 15:
                            # Check if we have valid fix (quality > 0) and coordinates
                            if parts[6] != '0' and parts[2] and parts[4]:
                                ser.close()
                                return "OK"
                else:
                    break  # No data available, exit early
            except:
                continue
        
        ser.close()
        return "FAIL"
    except:
        return "FAIL"

def check_camera_status():
    """Check camera module status"""
    try:
        # Try to initialize camera briefly
        picam2 = Picamera2()
        picam2.close()
        return "OK"
    except:
        return "FAIL"

def check_gyro_status():
    """Check ADXL345 gyro/accelerometer status"""
    try:
        # Try to access ADXL345 on I2C bus 1, address 0x53
        bus = smbus2.SMBus(1)
        # Try to read device ID or a register
        bus.read_byte_data(0x53, 0x00)  # Read DEVID register
        bus.close()
        return "OK"
    except:
        return "FAIL"

def display_status_page():
    """Display system status landing page with real-time updates"""
    print("Displaying status landing page with live updates...")
    
    # Initialize button state
    last_press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    
    # Counter for timing sensor checks (check every ~3 seconds for less frequent blocking)
    update_counter = 0
    update_interval = 60  # Check sensors every 60 cycles (60 * 0.05s = 3s)
    
    # Initialize with first status check
    wifi_status = check_wifi_status()
    gps_status = check_gps_status()
    camera_status = check_camera_status()
    gyro_status = check_gyro_status()
    
    while True:
        # Update sensor status every 2 seconds
        if update_counter >= update_interval:
            wifi_status = check_wifi_status()
            gps_status = check_gps_status()
            camera_status = check_camera_status()
            gyro_status = check_gyro_status()
            update_counter = 0
        
        # Create display with current status
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # Status indicators with larger font and spacing
        y_pos = 20
        
        # WiFi Status
        draw.text((10, y_pos), "WiFi:", fill="WHITE", font=font_large)
        wifi_color = "GREEN" if wifi_status == "OK" else "RED"
        draw.text((120, y_pos), wifi_status, fill=wifi_color, font=font_large)
        
        y_pos += 40
        
        # GPS Status
        draw.text((10, y_pos), "GPS:", fill="WHITE", font=font_large)
        gps_color = "GREEN" if gps_status == "OK" else "RED"
        draw.text((120, y_pos), gps_status, fill=gps_color, font=font_large)
        
        y_pos += 40
        
        # Camera Status
        draw.text((10, y_pos), "Camera:", fill="WHITE", font=font_large)
        camera_color = "GREEN" if camera_status == "OK" else "RED"
        draw.text((139, y_pos), camera_status, fill=camera_color, font=font_large)
        
        y_pos += 40
        
        # Gyro Status
        draw.text((10, y_pos), "Gyro:", fill="WHITE", font=font_large)
        gyro_color = "GREEN" if gyro_status == "OK" else "RED"
        draw.text((120, y_pos), gyro_status, fill=gyro_color, font=font_large)
        
        # Instructions
        draw.text((10, 190), "Press ENTER", fill="CYAN", font=font_large)
        draw.text((10, 215), "to continue", fill="CYAN", font=font_large)
        
        # Display image
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        
        # Check for ENTER button press (responsive) - LCD or Pico2
        press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        if (press_state == 0 and last_press_state == 1) or pico2_button_states['press_pressed']:
            if pico2_button_states['press_pressed']:
                pico2_button_states['press_pressed'] = False  # Reset Pico2 state
            time.sleep(0.1)  # Debounce
            break
        last_press_state = press_state
        
        # Shorter sleep for more responsive button checking
        time.sleep(0.05)
        update_counter += 1

# Splash screen
image = Image.open('./images/output.png')
im_r = image.rotate(270)
lcd.ShowImage(im_r)

# Send splash command to Pico2 (skip large image for now)
#send_pico2_command("SPLASH")

time.sleep(2)

# Show status page after splash
display_status_page()

# Menu setup
menu_items = ['Gyro', 'AI-Camera', 'Gps', 'Traffic', 'Bluetooth', 'Go FLY']
selection_index = -1
last_down_state = -1
last_up_state = -1
last_press_state = -1
last_key1_state = -1
last_key2_state = -1

# Pico2 button state tracking for edge detection
last_pico2_states = {
    'up_pressed': False,
    'down_pressed': False,
    'left_pressed': False,
    'right_pressed': False,
    'press_pressed': False,
    'key1_pressed': False,
    'key2_pressed': False
}
label_positions = [
    (5, 5, 235, 30),
    (5, 32, 235, 57),
    (5, 59, 235, 84),
    (5, 86, 235, 111),
    (5, 113, 235, 138),
    (5, 140, 235, 165)
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
    key1_state = lcd.digital_read(lcd.GPIO_KEY1_PIN)
    key2_state = lcd.digital_read(lcd.GPIO_KEY2_PIN)

    # Down button press (LCD or Pico2)
    pico2_down_pressed = pico2_button_states['down_pressed'] and not last_pico2_states['down_pressed']
    if (down_state == 0 and last_down_state == 1) or pico2_down_pressed:
        selection_index = (selection_index + 1) % len(menu_items)
        update_menu_display(selection_index)
        print("Joystick DOWN")

    # Up button press (LCD or Pico2)
    pico2_up_pressed = pico2_button_states['up_pressed'] and not last_pico2_states['up_pressed']
    if (up_state == 0 and last_up_state == 1) or pico2_up_pressed:
        selection_index = (selection_index - 1) % len(menu_items)
        update_menu_display(selection_index)
        print("Joystick UP")

    # Center press button (SELECT) (LCD or Pico2)
    pico2_press_pressed = pico2_button_states['press_pressed'] and not last_pico2_states['press_pressed']
    if (press_state == 0 and last_press_state == 1) or pico2_press_pressed:
        print(f"SELECTED: {menu_items[selection_index]}")
        if menu_items[selection_index] == "Gyro":
            display_setup_page(lcd, font4)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "AI-Camera":
            display_stream_page(lcd)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "Gps":
            display_gps_page(lcd)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "Traffic":
            display_traffic_page(lcd)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "Bluetooth":
            display_bluetooth_page(lcd)
            update_menu_display(selection_index)  # Return to menu
        
        if menu_items[selection_index] == "Go FLY":
            display_fly_page(lcd, font4)
            update_menu_display(selection_index)  # Return to menu

    # KEY1 button press - direct access to Gyro page (LCD or Pico2)
    pico2_key1_pressed = pico2_button_states['key1_pressed'] and not last_pico2_states['key1_pressed']
    if (key1_state == 0 and last_key1_state == 1) or pico2_key1_pressed:
        print("KEY1 pressed - Opening Gyro page")
        display_setup_page(lcd, font4)
        update_menu_display(selection_index)  # Return to menu

    # KEY2 button press - direct access to Streaming page (LCD or Pico2)
    pico2_key2_pressed = pico2_button_states['key2_pressed'] and not last_pico2_states['key2_pressed']
    if (key2_state == 0 and last_key2_state == 1) or pico2_key2_pressed:
        print("KEY2 pressed - Opening Streaming page")
        display_stream_page(lcd)
        update_menu_display(selection_index)  # Return to menu

    # Update LCD button states
    last_down_state = down_state
    last_up_state = up_state
    last_press_state = press_state
    last_key1_state = key1_state
    last_key2_state = key2_state

    # Update Pico2 button states for edge detection
    last_pico2_states = pico2_button_states.copy()

    time.sleep(0.1)