from PIL import Image, ImageDraw, ImageFont
import time
import subprocess
import threading

def run_command(command):
    """Run shell command and return output"""
    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True, timeout=15)
        return result.returncode == 0, result.stdout.strip() + "\n" + result.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "Command timeout"
    except Exception as e:
        return False, str(e)

def get_bluetooth_status():
    """Get current bluetooth status"""
    success, output = run_command("bluetoothctl show")
    if success and "Powered: yes" in output:
        return "ON"
    return "OFF"

def get_paired_devices():
    """Get list of paired devices with actual names"""
    import re
    success, output = run_command("bluetoothctl paired-devices")
    if success:
        devices = []
        for line in output.split('\n'):
            if line.startswith('Device'):
                parts = line.split(' ', 2)
                if len(parts) >= 3:
                    mac = parts[1]
                    name = parts[2] if len(parts) > 2 else "Unknown Device"
                    
                    # Try to get the actual device name using bluetoothctl info
                    info_success, info_output = run_command(f"bluetoothctl info {mac}")
                    if info_success:
                        for info_line in info_output.split('\n'):
                            if 'Name:' in info_line:
                                actual_name = info_line.split('Name:', 1)[1].strip()
                                if actual_name and actual_name != mac:
                                    name = actual_name
                                break
                    
                    # Only add devices that don't have MAC-like names (XX-XX-XX format)
                    if not re.match(r'^[A-Fa-f0-9]{2}[:-][A-Fa-f0-9]{2}[:-]', name):
                        devices.append((mac, name))
        return devices
    return []

def start_discovery():
    """Start bluetooth device discovery"""
    run_command("bluetoothctl scan on")

def stop_discovery():
    """Stop bluetooth device discovery"""
    run_command("bluetoothctl scan off")

def get_discovered_devices():
    """Get list of discovered devices with actual names"""
    import re
    success, output = run_command("bluetoothctl devices")
    if success:
        devices = []
        for line in output.split('\n'):
            if line.startswith('Device'):
                parts = line.split(' ', 2)
                if len(parts) >= 3:
                    mac = parts[1]
                    name = parts[2] if len(parts) > 2 else "Unknown Device"
                    
                    # Try to get the actual device name using bluetoothctl info
                    info_success, info_output = run_command(f"bluetoothctl info {mac}")
                    if info_success:
                        for info_line in info_output.split('\n'):
                            if 'Name:' in info_line:
                                actual_name = info_line.split('Name:', 1)[1].strip()
                                if actual_name and actual_name != mac:
                                    name = actual_name
                                break
                    
                    # Only add devices that don't have MAC-like names (XX-XX-XX format)
                    if not re.match(r'^[A-Fa-f0-9]{2}[:-][A-Fa-f0-9]{2}[:-]', name):
                        devices.append((mac, name))
        return devices
    return []

def pair_device(mac_address):
    """Pair with device"""
    # First, make sure agent is on and default agent is set
    run_command("bluetoothctl agent on")
    run_command("bluetoothctl default-agent")
    
    # Trust the device first
    run_command(f"bluetoothctl trust {mac_address}")
    
    # Attempt to pair
    success, output = run_command(f"bluetoothctl pair {mac_address}")
    print(f"Pairing output: {output}")
    
    if not success:
        # If pairing fails, try connecting directly (sometimes works for already paired devices)
        success, output = run_command(f"bluetoothctl connect {mac_address}")
        print(f"Connect attempt output: {output}")
    
    return success

def connect_device(mac_address):
    """Connect to paired device"""
    success, output = run_command(f"bluetoothctl connect {mac_address}")
    return success

def display_bluetooth_page(lcd):
    """Display bluetooth pairing page"""

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

    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 20)
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 14)
    
    menu_options = ["SCAN", "PAIR", "CONNECT", "STATUS", "BACK"]
    selected_option = 0
    
    # State variables
    scanning = False
    discovered_devices = []
    paired_devices = []
    device_selection = 0
    in_device_menu = False
    status_message = ""
    message_timer = 0
    
    last_up = 1
    last_down = 1
    last_left = 1
    last_right = 1
    last_press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)

    # Pico2 button state tracking for edge detection
    # Initialize with current state to avoid immediate trigger from menu selection
    last_pico2_states = pico2_button_states.copy()
    # Ensure all expected keys exist in last_pico2_states
    for key in ['up_pressed', 'down_pressed', 'left_pressed', 'right_pressed', 'press_pressed', 'key1_pressed', 'key2_pressed']:
        if key not in last_pico2_states:
            last_pico2_states[key] = False

    # Get initial bluetooth status
    bt_status = get_bluetooth_status()
    paired_devices = get_paired_devices()

    # Small delay to avoid immediate exit from menu selection button press
    time.sleep(0.2)

    while True:
        # Create fresh image
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # Handle joystick input (LCD or Pico2)
        up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        pico2_up_pressed = pico2_button_states['up_pressed'] and not last_pico2_states['up_pressed']
        if (up == 0 and last_up == 1) or pico2_up_pressed:
            if in_device_menu:
                device_selection = max(0, device_selection - 1)
                print(f"UP pressed - device_selection: {device_selection}")
            else:
                selected_option = (selected_option - 1) % len(menu_options)
        last_up = up
        
        down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
        pico2_down_pressed = pico2_button_states['down_pressed'] and not last_pico2_states['down_pressed']
        if (down == 0 and last_down == 1) or pico2_down_pressed:
            if in_device_menu:
                # Get correct device list based on current menu option
                if selected_option == 0 or selected_option == 1:  # SCAN or PAIR
                    max_devices = len(discovered_devices)
                else:  # CONNECT
                    max_devices = len(paired_devices)
                device_selection = min(max_devices - 1, device_selection + 1) if max_devices > 0 else 0
                print(f"DOWN pressed - device_selection: {device_selection}, max_devices: {max_devices}")
            else:
                selected_option = (selected_option + 1) % len(menu_options)
        last_down = down
        
        left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
        if left == 0 and last_left == 1:
            if in_device_menu:
                in_device_menu = False
                device_selection = 0
        last_left = left
        
        press = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        pico2_press_pressed = pico2_button_states['press_pressed'] and not last_pico2_states['press_pressed']
        if (press == 0 and last_press == 1) or pico2_press_pressed:
            if in_device_menu:
                # Handle device selection actions
                if selected_option == 0 and discovered_devices:  # SCAN - pair selected device
                    mac, name = discovered_devices[device_selection]
                    if pair_device(mac):
                        status_message = f"Paired: {name[:20]}"
                        paired_devices = get_paired_devices()
                    else:
                        status_message = "Pairing failed"
                    message_timer = time.time()
                    in_device_menu = False
                    scanning = False
                    stop_discovery()
                elif selected_option == 1 and discovered_devices:  # PAIR
                    mac, name = discovered_devices[device_selection]
                    if pair_device(mac):
                        status_message = f"Paired: {name[:20]}"
                        paired_devices = get_paired_devices()
                    else:
                        status_message = "Pairing failed"
                    message_timer = time.time()
                    in_device_menu = False
                elif selected_option == 2 and paired_devices:  # CONNECT
                    mac, name = paired_devices[device_selection]
                    if connect_device(mac):
                        status_message = f"Connected: {name[:20]}"
                    else:
                        status_message = "Connection failed"
                    message_timer = time.time()
                    in_device_menu = False
            else:
                # Handle main menu actions
                if selected_option == 4:  # BACK
                    if scanning:
                        stop_discovery()
                    return
                elif selected_option == 0:  # SCAN
                    if not scanning:
                        scanning = True
                        start_discovery()
                        status_message = "Scanning..."
                        message_timer = time.time()
                        def update_devices():
                            time.sleep(5)
                            nonlocal discovered_devices
                            discovered_devices = get_discovered_devices()
                        threading.Thread(target=update_devices).start()
                    else:
                        # If already scanning, allow device selection
                        if discovered_devices:
                            in_device_menu = True
                            device_selection = 0
                            print("Entering device selection mode")
                elif selected_option == 1:  # PAIR
                    discovered_devices = get_discovered_devices()
                    if discovered_devices:
                        in_device_menu = True
                        device_selection = 0
                    else:
                        status_message = "No devices found"
                        message_timer = time.time()
                elif selected_option == 2:  # CONNECT
                    paired_devices = get_paired_devices()
                    if paired_devices:
                        in_device_menu = True
                        device_selection = 0
                    else:
                        status_message = "No paired devices"
                        message_timer = time.time()
                elif selected_option == 3:  # STATUS
                    bt_status = get_bluetooth_status()
                    paired_devices = get_paired_devices()
                    status_message = f"BT: {bt_status}"
                    message_timer = time.time()
        last_press = press
        
        # Update discovered devices periodically when scanning
        if scanning:
            if time.time() % 2 < 0.1:  # Update every 2 seconds
                new_devices = get_discovered_devices()
                if len(new_devices) != len(discovered_devices):
                    discovered_devices = new_devices
        
        # Draw title
        draw.text((10, 10), "BLUETOOTH", fill="MAGENTA", font=font_large)
        
        # Draw status message if active
        if status_message and time.time() - message_timer < 3:
            draw.text((10, 35), status_message, fill="YELLOW", font=font_small)
        
        if in_device_menu or (selected_option == 0 and scanning and discovered_devices):
            # Draw device list
            devices = discovered_devices if selected_option == 0 or selected_option == 1 else paired_devices
            action = "SCAN" if selected_option == 0 else ("PAIR" if selected_option == 1 else "CONNECT")
            
            draw.text((10, 60), f"{action} DEVICES:", fill="CYAN", font=font_small)
            
            start_y = 80
            max_devices = min(len(devices), 8)  # Show max 8 devices
            for i in range(max_devices):
                mac, name = devices[i]
                color = "YELLOW" if i == device_selection and in_device_menu else "WHITE"
                # Show device name, truncated if too long
                display_name = name if len(name) <= 22 else name[:19] + "..."
                draw.text((15, start_y + i * 18), f"{i+1}. {display_name}", fill=color, font=font_small)
                
            # Debug info
            if in_device_menu:
                draw.text((150, 10), f"Sel:{device_selection}", fill="RED", font=font_small)
            
            if len(devices) == 0:
                draw.text((15, start_y), "No devices found", fill="WHITE", font=font_small)
            elif len(devices) > 8:
                draw.text((15, start_y + 8 * 18), f"...{len(devices)-8} more", fill="GRAY", font=font_small)
            
            if in_device_menu:
                draw.text((10, 220), "PRESS: Select  LEFT: Back", fill="CYAN", font=font_small)
            else:
                draw.text((10, 220), "PRESS: Select devices", fill="YELLOW", font=font_small)
        else:
            # Draw main menu
            start_y = 60
            for i, option in enumerate(menu_options):
                color = "YELLOW" if i == selected_option else "WHITE"
                draw.text((20, start_y + i * 25), option, fill=color, font=font_large)
            
            # Draw bluetooth status
            draw.text((10, 200), f"Status: {bt_status}", fill="CYAN", font=font_small)
            draw.text((10, 215), f"Paired: {len(paired_devices)}", fill="CYAN", font=font_small)
            
            if scanning:
                draw.text((120, 200), "SCANNING...", fill="YELLOW", font=font_small)
        
        # Update Pico2 button states for edge detection
        last_pico2_states = pico2_button_states.copy()

        # Display rotated image
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
        time.sleep(0.1)