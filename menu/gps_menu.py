import serial
import time
import sys
import os
from PIL import Image, ImageDraw, ImageFont
from gpiozero import DigitalOutputDevice
from library import ST7789

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT

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
    
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 20)
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 16)
    
    # Initialize GPS enable pin
    gps_en = None
    if GPS_EN_PIN is not None:
        try:
            gps_en = DigitalOutputDevice(GPS_EN_PIN)
            gps_en.on()
            time.sleep(2)  # Wait for GPS to boot
        except Exception as e:
            print(f"Error setting up GPS enable pin: {e}")
            gps_en = None
    
    # Open GPS serial port
    try:
        ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=GPS_TIMEOUT)
        print(f"GPS serial port opened: {GPS_PORT}")
    except serial.SerialException as e:
        print(f"Error opening GPS port: {e}")
        # Display error and return
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        draw.text((10, 50), "GPS ERROR", fill="RED", font=font_large)
        draw.text((10, 80), "Port not", fill="WHITE", font=font_small)
        draw.text((10, 100), "available", fill="WHITE", font=font_small)
        draw.text((10, 180), "Press CENTER", fill="CYAN", font=font_small)
        draw.text((10, 200), "to exit", fill="CYAN", font=font_small)
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
    
    try:
        while True:
            # Read GPS data
            try:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    gps_data = parse_gga(line)
                    if gps_data:
                        last_gps_data = gps_data
                        data_timeout = time.time()
            except Exception as e:
                print(f"GPS read error: {e}")
            
            # Create display
            background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
            draw = ImageDraw.Draw(background)
            
            # Title
            draw.text((10, 5), "GPS POSITION", fill="CYAN", font=font_large)
            
            # Check if we have recent data (within 5 seconds)
            if last_gps_data and (time.time() - data_timeout < 5):
                # Display coordinates
                lat = last_gps_data['latitude']
                lon = last_gps_data['longitude']
                sats = last_gps_data['satellites']
                
                if lat is not None and lon is not None:
                    draw.text((10, 40), "LATITUDE:", fill="WHITE", font=font_small)
                    draw.text((10, 60), f"{lat:.6f}", fill="GREEN", font=font_small)
                    
                    draw.text((10, 90), "LONGITUDE:", fill="WHITE", font=font_small)
                    draw.text((10, 110), f"{lon:.6f}", fill="GREEN", font=font_small)
                    
                    draw.text((10, 140), f"SATS: {sats}", fill="YELLOW", font=font_small)
                else:
                    draw.text((10, 60), "NO VALID", fill="RED", font=font_small)
                    draw.text((10, 80), "COORDINATES", fill="RED", font=font_small)
            else:
                # No GPS data
                draw.text((10, 60), "NO GPS DATA", fill="RED", font=font_large)
                draw.text((10, 90), "Searching...", fill="YELLOW", font=font_small)
            
            # Instructions
            draw.text((10, 180), "Press CENTER", fill="CYAN", font=font_small)
            draw.text((10, 200), "to exit", fill="CYAN", font=font_small)
            
            # Display image
            im_r = background.rotate(270)
            lcd.ShowImage(im_r)
            
            # Check for exit button
            if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
                time.sleep(0.1)
                if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
                    break
            
            time.sleep(0.1)
            
    finally:
        ser.close()
        if gps_en is not None:
            gps_en.off()
            gps_en.close()
        lcd.clear()
        print("GPS page closed")