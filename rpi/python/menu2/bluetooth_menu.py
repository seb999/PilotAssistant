import time
from PIL import Image, ImageDraw, ImageFont
import subprocess


def display_bluetooth_menu(lcd):
    """Display Bluetooth menu page"""
    print("Displaying Bluetooth menu...")

    # Fonts
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 18)
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 14)

    # Initialize button states
    last_up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    last_down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
    last_press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_key1_state = lcd.digital_read(lcd.GPIO_KEY1_PIN)

    # Menu options
    menu_options = ['Scan Devices', 'Paired Devices', 'Bluetooth Status', 'Back']
    selected_option = 0

    def check_bluetooth_status():
        """Check if Bluetooth is enabled"""
        try:
            result = subprocess.run(['bluetoothctl', 'show'], capture_output=True, text=True, timeout=2)
            return "Powered: yes" in result.stdout
        except:
            return False

    def scan_bluetooth_devices():
        """Scan for nearby Bluetooth devices"""
        try:
            # Start scanning
            subprocess.run(['bluetoothctl', 'scan', 'on'], timeout=1)
            time.sleep(3)  # Scan for 3 seconds

            # Get scan results
            result = subprocess.run(['bluetoothctl', 'devices'], capture_output=True, text=True, timeout=2)
            devices = []
            for line in result.stdout.split('\n'):
                if line.startswith('Device'):
                    parts = line.split(' ', 2)
                    if len(parts) >= 3:
                        devices.append(parts[2])  # Device name

            # Stop scanning
            subprocess.run(['bluetoothctl', 'scan', 'off'], timeout=1)
            return devices[:5]  # Return first 5 devices
        except:
            return ["Scan failed"]

    def get_paired_devices():
        """Get list of paired Bluetooth devices"""
        try:
            result = subprocess.run(['bluetoothctl', 'paired-devices'], capture_output=True, text=True, timeout=2)
            devices = []
            for line in result.stdout.split('\n'):
                if line.startswith('Device'):
                    parts = line.split(' ', 2)
                    if len(parts) >= 3:
                        devices.append(parts[2])  # Device name
            return devices[:5] if devices else ["No paired devices"]
        except:
            return ["Error reading devices"]

    def update_display():
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Title
        draw.text((10, 5), "BLUETOOTH", fill="MAGENTA", font=font_large)

        # Menu options
        y_pos = 40
        for i, option in enumerate(menu_options):
            color = "BLACK" if i == selected_option else "CYAN"
            bg_color = "CYAN" if i == selected_option else None

            if bg_color:
                text_bbox = draw.textbbox((0, 0), option, font=font_medium)
                text_width = text_bbox[2] - text_bbox[0]
                draw.rectangle((10, y_pos, 10 + text_width + 10, y_pos + 25), fill=bg_color)

            draw.text((15, y_pos), option, fill=color, font=font_medium)
            y_pos += 30

        # Instructions
        draw.text((10, 200), "UP/DOWN: Navigate", fill="CYAN", font=font_small)
        draw.text((10, 215), "ENTER: Select", fill="CYAN", font=font_small)

        # Display image
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

    def show_scan_results():
        """Show Bluetooth scan results"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "SCANNING...", fill="YELLOW", font=font_large)
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        devices = scan_bluetooth_devices()

        # Show results
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "FOUND DEVICES:", fill="MAGENTA", font=font_medium)

        y_pos = 35
        for device in devices:
            if y_pos > 180:
                break
            draw.text((10, y_pos), device[:25], fill="CYAN", font=font_small)  # Truncate long names
            y_pos += 20

        draw.text((10, 215), "ENTER: Back", fill="CYAN", font=font_small)

        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        # Wait for enter to go back
        while True:
            press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if press_state == 0:
                time.sleep(0.1)
                break
            time.sleep(0.1)

    def show_paired_devices():
        """Show paired Bluetooth devices"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "GETTING DEVICES...", fill="YELLOW", font=font_large)
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        devices = get_paired_devices()

        # Show results
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "PAIRED DEVICES:", fill="MAGENTA", font=font_medium)

        y_pos = 35
        for device in devices:
            if y_pos > 180:
                break
            draw.text((10, y_pos), device[:25], fill="CYAN", font=font_small)  # Truncate long names
            y_pos += 20

        draw.text((10, 215), "ENTER: Back", fill="CYAN", font=font_small)

        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        # Wait for enter to go back
        while True:
            press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if press_state == 0:
                time.sleep(0.1)
                break
            time.sleep(0.1)

    def show_bluetooth_status():
        """Show Bluetooth status"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "CHECKING STATUS...", fill="YELLOW", font=font_large)
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        bt_enabled = check_bluetooth_status()

        # Show status
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "BLUETOOTH STATUS:", fill="MAGENTA", font=font_medium)

        status_text = "ENABLED" if bt_enabled else "DISABLED"
        status_color = "GREEN" if bt_enabled else "RED"
        draw.text((10, 50), f"Status: {status_text}", fill=status_color, font=font_large)

        draw.text((10, 215), "ENTER: Back", fill="CYAN", font=font_small)

        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        # Wait for enter to go back
        while True:
            press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if press_state == 0:
                time.sleep(0.1)
                break
            time.sleep(0.1)

    # Main menu loop
    update_display()

    while True:
        up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
        press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        key1_state = lcd.digital_read(lcd.GPIO_KEY1_PIN)

        # Up button
        if up_state == 0 and last_up_state == 1:
            selected_option = (selected_option - 1) % len(menu_options)
            update_display()
            time.sleep(0.1)

        # Down button
        if down_state == 0 and last_down_state == 1:
            selected_option = (selected_option + 1) % len(menu_options)
            update_display()
            time.sleep(0.1)

        # Enter button
        if press_state == 0 and last_press_state == 1:
            if menu_options[selected_option] == 'Scan Devices':
                show_scan_results()
                update_display()
            elif menu_options[selected_option] == 'Paired Devices':
                show_paired_devices()
                update_display()
            elif menu_options[selected_option] == 'Bluetooth Status':
                show_bluetooth_status()
                update_display()
            elif menu_options[selected_option] == 'Back':
                break
            time.sleep(0.1)

        # KEY1 to go back
        if key1_state == 0 and last_key1_state == 1:
            break

        # Update button states
        last_up_state = up_state
        last_down_state = down_state
        last_press_state = press_state
        last_key1_state = key1_state

        time.sleep(0.1)