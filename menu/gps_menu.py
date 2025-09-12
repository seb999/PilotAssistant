import serial
import time
import sys
import os
from PIL import Image, ImageDraw, ImageFont
from gpiozero import DigitalOutputDevice
from library import ST7789

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT, DEBUG_MODE, DEBUG_LATITUDE, DEBUG_LONGITUDE

def parse_gga(nmea_line):
    """Parse GPGGA NMEA sentence for coordinates"""
    if not nmea_line.startswith('$GPGGA'):
        return None
    
    parts = nmea_line.split(',')
    if len(parts) < 15:
        return None
    
    # Check if we have valid data
    if parts[6] == '0':  # Fix quality 0 = no fix
        return None
    
    try:
        # Parse latitude
        if parts[2] and parts[3]:
            lat_raw = float(parts[2])
            lat_deg = int(lat_raw / 100)
            lat_min = lat_raw - (lat_deg * 100)
            latitude = lat_deg + (lat_min / 60)
            if parts[3] == 'S':
                latitude = -latitude
        else:
            latitude = None
        
        # Parse longitude
        if parts[4] and parts[5]:
            lon_raw = float(parts[4])
            lon_deg = int(lon_raw / 100)
            lon_min = lon_raw - (lon_deg * 100)
            longitude = lon_deg + (lon_min / 60)
            if parts[5] == 'W':
                longitude = -longitude
        else:
            longitude = None
        
        return {
            'latitude': latitude,
            'longitude': longitude,
            'satellites': parts[7] if parts[7] else '0'
        }
    except (ValueError, IndexError):
        return None

def display_gps_page(lcd):
    print("Displaying GPS page...")
    if DEBUG_MODE:
        print("DEBUG MODE: Using Stockholm coordinates")
    
    font_title = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 28)
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 20)
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 18)
    
    # Display initial GPS page immediately
    def display_initial_page(status_text="Initializing GPS..."):
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # Title with cyan background spanning full width
        title_text = "GPS POSITION"
        if DEBUG_MODE:
            title_text = "GPS DEBUG"
        title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
        title_height = title_bbox[3] - title_bbox[1]
        
        # Draw cyan background for title (full width)
        draw.rectangle((0, 5, lcd.width, title_height + 15), fill="CYAN")
        draw.text((10, 8), title_text, fill="BLACK", font=font_title)
        
        # Status message
        if DEBUG_MODE:
            status_text = "Debug Mode Active"
        draw.text((10, 70), status_text, fill="YELLOW", font=font_medium)
        
        # Instructions
        draw.text((10, 190), "Press CENTER", fill="CYAN", font=font_medium)
        draw.text((10, 215), "to exit", fill="CYAN", font=font_medium)
        
        # Display image
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
    
    # Show initial page immediately
    display_initial_page()
    
    # Handle debug mode or real GPS
    if DEBUG_MODE:
        # In debug mode, skip GPS initialization
        gps_en = None
        ser = None
        display_initial_page("Debug coordinates ready")
        # Create debug GPS data
        debug_gps_data = {
            'latitude': DEBUG_LATITUDE,
            'longitude': DEBUG_LONGITUDE,
            'satellites': '8'  # Simulate 8 satellites for debug
        }
    else:
        # Initialize GPS enable pin in background
        gps_en = None
        if GPS_EN_PIN is not None:
            try:
                gps_en = DigitalOutputDevice(GPS_EN_PIN)
                gps_en.on()
                display_initial_page("GPS powering up...")
                time.sleep(2)  # Wait for GPS to boot
                display_initial_page("Opening GPS port...")
            except Exception as e:
                print(f"Error setting up GPS enable pin: {e}")
                gps_en = None
        
        # Open GPS serial port
        try:
            ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=GPS_TIMEOUT)
            print(f"GPS serial port opened: {GPS_PORT}")
            display_initial_page("Searching for satellites...")
        except serial.SerialException as e:
            print(f"Error opening GPS port: {e}")
            # Display error and return
            background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
            draw = ImageDraw.Draw(background)
            # Title with cyan background spanning full width
            title_text = "GPS POSITION"
            title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
            title_height = title_bbox[3] - title_bbox[1]
            draw.rectangle((0, 5, lcd.width, title_height + 15), fill="CYAN")
            draw.text((10, 8), title_text, fill="BLACK", font=font_title)
            
            draw.text((10, 50), "GPS ERROR", fill="RED", font=font_large)
            draw.text((10, 80), "Port not", fill="WHITE", font=font_medium)
            draw.text((10, 100), "available", fill="WHITE", font=font_medium)
            draw.text((10, 180), "Press CENTER", fill="CYAN", font=font_medium)
            draw.text((10, 200), "to exit", fill="CYAN", font=font_medium)
            im_r = background.rotate(270)
            lcd.ShowImage(im_r)
            
            # Wait for button press to exit
            while True:
                if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
                    time.sleep(0.1)
                    if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
                        break
                time.sleep(0.1)
            return
    
    last_gps_data = None
    data_timeout = time.time()
    
    display_update_counter = 0
    # Initialize with current button state to avoid immediate exit
    last_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    
    try:
        while True:
            # Check for exit button (responsive input)
            current_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if current_state == 0 and last_state == 1:
                time.sleep(0.05)  # Debounce
                if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
                    break
            last_state = current_state
            
            # Handle GPS data based on mode
            if DEBUG_MODE:
                # Use debug GPS data
                last_gps_data = debug_gps_data
                data_timeout = time.time()
            else:
                # Read GPS data only if available (non-blocking)
                try:
                    if ser.in_waiting > 0:
                        line = ser.readline().decode('ascii', errors='replace').strip()
                        if line:
                            gps_data = parse_gga(line)
                            if gps_data:
                                last_gps_data = gps_data
                                data_timeout = time.time()
                except Exception as e:
                    print(f"GPS read error: {e}")
            
            # Update display every 5 cycles to reduce flicker while maintaining responsiveness
            display_update_counter += 1
            if display_update_counter >= 5:
                display_update_counter = 0
                
                # Create display
                background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
                draw = ImageDraw.Draw(background)
                
                # Title with cyan background spanning full width
                title_text = "GPS DEBUG" if DEBUG_MODE else "GPS POSITION"
                title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
                title_height = title_bbox[3] - title_bbox[1]
                
                # Draw cyan background for title (full width)
                draw.rectangle((0, 5, lcd.width, title_height + 15), fill="CYAN")
                draw.text((10, 8), title_text, fill="BLACK", font=font_title)
                
                # Check if we have recent data (within 5 seconds)
                if last_gps_data and (time.time() - data_timeout < 5):
                    # Display coordinates
                    lat = last_gps_data['latitude']
                    lon = last_gps_data['longitude']
                    sats = last_gps_data['satellites']
                    
                    if lat is not None and lon is not None:
                        draw.text((10, 50), "LATITUDE:", fill="WHITE", font=font_medium)
                        draw.text((10, 75), f"{lat:.6f}", fill="GREEN", font=font_large)
                        
                        draw.text((10, 105), "LONGITUDE:", fill="WHITE", font=font_medium)
                        draw.text((10, 130), f"{lon:.6f}", fill="GREEN", font=font_large)
                        
                        draw.text((10, 160), f"SATS: {sats}", fill="YELLOW", font=font_medium)
                    else:
                        draw.text((10, 70), "NO VALID", fill="RED", font=font_medium)
                        draw.text((10, 95), "COORDINATES", fill="RED", font=font_medium)
                else:
                    # No GPS data
                    draw.text((10, 70), "NO GPS DATA", fill="RED", font=font_large)
                    draw.text((10, 100), "Searching...", fill="YELLOW", font=font_medium)
                
                # Instructions
                draw.text((10, 190), "Press CENTER", fill="CYAN", font=font_medium)
                draw.text((10, 215), "to exit", fill="CYAN", font=font_medium)
                
                # Display image
                im_r = background.rotate(270)
                lcd.ShowImage(im_r)
            
            # Short sleep for responsive input checking
            time.sleep(0.02)
            
    finally:
        if ser is not None:
            ser.close()
        if gps_en is not None:
            gps_en.off()
            gps_en.close()
        lcd.clear()
        print("GPS page closed")