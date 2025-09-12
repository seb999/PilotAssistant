import requests
import time
import serial
import sys
import os
import subprocess
from math import sin, cos, sqrt, atan2, radians
from PIL import Image, ImageDraw, ImageFont
from gpiozero import DigitalOutputDevice

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT, DEBUG_MODE, DEBUG_LATITUDE, DEBUG_LONGITUDE

# Traffic menu configuration
AIRCRAFT_RANGE_KM = 25      # Radius in km to search for aircraft (good for Cessna at 100kts)
UPDATE_INTERVAL_SEC = 10    # Update aircraft data every N seconds

def haversine(lat1, lon1, lat2, lon2):
    """Calculate distance between two lat/lon points using Haversine formula"""
    R = 6371.0  # Radius of Earth in km
    dlat = radians(lat2 - lat1)
    dlon = radians(lon2 - lon1)
    a = sin(dlat / 2)**2 + cos(radians(lat1)) * cos(radians(lat2)) * sin(dlon / 2)**2
    c = 2 * atan2(sqrt(a), sqrt(1 - a))
    return R * c

def check_wifi_status():
    """Check WiFi connection status"""
    try:
        # Check if we have an IP address (indicating network connection)
        result = subprocess.run(['hostname', '-I'], capture_output=True, text=True, timeout=1)
        if result.returncode == 0 and result.stdout.strip():
            return True
        else:
            return False
    except:
        return False

def get_debug_location():
    """Get Stockholm coordinates for debug mode"""
    if DEBUG_MODE:
        print("Using debug coordinates (Stockholm)")
        return DEBUG_LATITUDE, DEBUG_LONGITUDE
    else:
        print("Debug mode disabled - GPS-only mode")
        return None

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

def get_gps_location():
    """Try to get GPS location, return None if not available"""
    gps_en = None
    if GPS_EN_PIN is not None:
        try:
            gps_en = DigitalOutputDevice(GPS_EN_PIN)
            gps_en.on()
            time.sleep(1)  # Brief GPS boot time
        except Exception as e:
            print(f"Error setting up GPS enable pin: {e}")
            gps_en = None
    
    try:
        ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=1)
        # Try to get GPS data for 3 seconds max
        start_time = time.time()
        while time.time() - start_time < 3:
            if ser.in_waiting > 0:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    gps_data = parse_gga(line)
                    if gps_data and gps_data['latitude'] is not None and gps_data['longitude'] is not None:
                        ser.close()
                        if gps_en is not None:
                            gps_en.off()
                            gps_en.close()
                        return gps_data['latitude'], gps_data['longitude']
        ser.close()
    except Exception as e:
        print(f"GPS error: {e}")
    
    if gps_en is not None:
        gps_en.off()
        gps_en.close()
    
    return None

def get_aircraft(lat, lon, radius_km=AIRCRAFT_RANGE_KM):
    """Get aircraft data within bounding box using OpenSky Network API"""
    try:
        delta_deg = radius_km / 111  # Rough approximation
        url = f"https://opensky-network.org/api/states/all?lamin={lat - delta_deg}&lamax={lat + delta_deg}&lomin={lon - delta_deg}&lomax={lon + delta_deg}"
        response = requests.get(url, timeout=10)
        data = response.json()
        aircraft = []

        for state in data.get("states", []):
            ac_lat = state[6]
            ac_lon = state[5]
            heading = state[10]  # True track (heading) in degrees
            callsign = state[1].strip() if state[1] else "N/A"
            altitude = state[7]  # Barometric altitude in meters
            velocity = state[9]  # Velocity in m/s
            
            if ac_lat and ac_lon and heading is not None:
                dist = haversine(lat, lon, ac_lat, ac_lon)
                if dist <= radius_km:
                    speed_knots = int(velocity * 1.94384) if velocity else 0  # Convert m/s to knots
                    aircraft.append({
                        "callsign": callsign,
                        "lat": ac_lat,
                        "lon": ac_lon,
                        "heading": heading,
                        "altitude": altitude if altitude else 0,
                        "speed_knots": speed_knots,
                        "distance_km": round(dist, 2)
                    })
        return aircraft
    except Exception as e:
        print(f"Aircraft data error: {e}")
        return []

def lat_lon_to_xy(lat, lon, center_lat, center_lon, center_y=130, scale=2.0):
    """Convert lat/lon to x,y coordinates for display (centered on user position)"""
    # Simple Mercator-like projection for small areas
    x = (lon - center_lon) * 111 * cos(radians(center_lat)) * scale + 120
    y = (center_lat - lat) * 111 * scale + center_y  # Inverted Y for screen coordinates
    return int(x), int(y)

def display_traffic_page(lcd):
    """Display aircraft traffic visualization page with GPS status"""
    print("Displaying traffic page...")
    
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 14)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 18)
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    font_tiny = ImageFont.load_default()
    
    # Initialize button state tracking
    last_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    last_down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
    
    # Initialize variables
    user_lat, user_lon, pos_source = None, None, "LOADING"
    aircraft_list = []
    last_update = 0
    update_interval = UPDATE_INTERVAL_SEC
    position_fetched = False
    
    # GPS status variables
    gps_status = "RED"  # Start with red (no signal)
    gps_data = None
    
    # WiFi/4G status variables
    wifi_connected = False
    
    # Aircraft info display variables
    show_aircraft_info = False
    aircraft_info_timeout = 0
    selected_aircraft = None
    current_aircraft_index = 0
    sorted_aircraft_list = []
    
    def draw_display(status_text="Loading..."):
        """Draw the traffic display"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # GPS Status - red when using IP location, green when GPS coordinates available
        gps_color = "GREEN" if gps_status == "GREEN" else "RED"
        draw.text((10, 5), "GPS", fill=gps_color, font=font_large)
        
        # WiFi Status - green if connected to wifi, red otherwise - positioned top right
        wifi_color = "GREEN" if wifi_connected else "RED"
        wifi_text = "WIFI"
        # Calculate text width to position from right edge
        wifi_bbox = draw.textbbox((0, 0), wifi_text, font=font_large)
        wifi_width = wifi_bbox[2] - wifi_bbox[0]
        wifi_x = lcd.width - wifi_width - 10  # 10 pixels from right edge
        draw.text((wifi_x, 5), wifi_text, fill=wifi_color, font=font_large)
        
        # Draw range circles (50km and 25km) if we have position - scaled up
        if user_lat and user_lon:
            center_x, center_y = 120, 130
            draw.ellipse([center_x-100, center_y-100, center_x+100, center_y+100], outline="DARKGRAY")  # ~50km
            draw.ellipse([center_x-50, center_y-50, center_x+50, center_y+50], outline="DARKGRAY")  # ~25km
            
            # Draw user position as bigger white aircraft symbol
            # Draw larger aircraft shape pointing north
            aircraft_points = [
                (center_x, center_y - 6),  # nose
                (center_x - 3, center_y + 3),  # left wing
                (center_x - 2, center_y + 5),  # left tail
                (center_x, center_y + 3),   # center tail
                (center_x + 2, center_y + 5),  # right tail
                (center_x + 3, center_y + 3),  # right wing
            ]
            draw.polygon(aircraft_points, fill="WHITE")
            
            # Draw aircraft as bigger dots only
            for aircraft in aircraft_list:
                ac_x, ac_y = lat_lon_to_xy(aircraft['lat'], aircraft['lon'], user_lat, user_lon, center_y)
                
                # Only draw aircraft that are within display bounds
                if 10 <= ac_x <= 230 and 10 <= ac_y <= 230:
                    # Check if this is the selected aircraft
                    if show_aircraft_info and selected_aircraft and aircraft['callsign'] == selected_aircraft['callsign']:
                        # Draw selected aircraft as red dot
                        dot_color = "RED"
                        line_color = "RED"
                    else:
                        # Draw aircraft as green dot (changed from yellow)
                        dot_color = "GREEN"
                        line_color = "GREEN"
                    
                    # Draw aircraft dot
                    draw.ellipse([ac_x-6, ac_y-6, ac_x+6, ac_y+6], fill=dot_color)
                    
                    # Draw direction line (heading indicator)
                    if aircraft['heading'] is not None:
                        line_length = 12
                        end_x = ac_x + line_length * sin(radians(aircraft['heading']))
                        end_y = ac_y - line_length * cos(radians(aircraft['heading']))
                        draw.line([ac_x, ac_y, end_x, end_y], fill=line_color, width=2)
        
        # Show selected aircraft info at bottom left with bigger characters
        if show_aircraft_info and selected_aircraft:
            # Position at bottom left - moved higher for better visibility
            info_x = 10
            info_y = 160
            
            # Get text values
            callsign_text = f"{selected_aircraft['callsign']}"
            altitude_m = selected_aircraft['altitude']
            altitude_ft = int(altitude_m * 3.28084) if altitude_m else 0
            altitude_text = f"{altitude_ft}ft"
            speed_text = f"{selected_aircraft['speed_knots']}kts"
            
            # Draw the texts aligned to bottom left
            draw.text((info_x, info_y), callsign_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 25), altitude_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 50), speed_text, fill="CYAN", font=font_large)
        
        # Display image
        im_r = background.rotate(270)
        lcd.ShowImage(im_r)
    
    # Show initial display immediately
    draw_display("Loading position...")
    
    try:
        while True:
            # Check for exit button
            current_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if current_state == 0 and last_state == 1:
                time.sleep(0.05)  # Debounce
                if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
                    break
            last_state = current_state
            
            # Update sorted aircraft list when aircraft data changes
            if aircraft_list:
                sorted_aircraft_list = sorted(aircraft_list, key=lambda ac: ac['distance_km'])
            
            # Check for UP button - next aircraft
            up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
            if up_state == 0 and last_up_state == 1:
                time.sleep(0.05)  # Debounce
                if lcd.digital_read(lcd.GPIO_KEY_UP_PIN) == 0 and sorted_aircraft_list:
                    # Go to next aircraft (forward in list)
                    current_aircraft_index = (current_aircraft_index + 1) % len(sorted_aircraft_list)
                    selected_aircraft = sorted_aircraft_list[current_aircraft_index]
                    show_aircraft_info = True
                    aircraft_info_timeout = time.time() + 15  # Show for 15 seconds
            last_up_state = up_state
            
            # Check for DOWN button - previous aircraft
            down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
            if down_state == 0 and last_down_state == 1:
                time.sleep(0.05)  # Debounce
                if lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN) == 0 and sorted_aircraft_list:
                    # Go to previous aircraft (backward in list)
                    current_aircraft_index = (current_aircraft_index - 1) % len(sorted_aircraft_list)
                    selected_aircraft = sorted_aircraft_list[current_aircraft_index]
                    show_aircraft_info = True
                    aircraft_info_timeout = time.time() + 15  # Show for 15 seconds
            last_down_state = down_state
            
            # Hide aircraft info after timeout
            if show_aircraft_info and time.time() > aircraft_info_timeout:
                show_aircraft_info = False
            
            # Check WiFi status periodically
            wifi_connected = check_wifi_status()
            
            # Get user position if not fetched yet
            if not position_fetched:
                if DEBUG_MODE:
                    draw_display("Using debug location...")
                    debug_pos = get_debug_location()
                    user_lat, user_lon, pos_source = debug_pos[0], debug_pos[1], "DEBUG"
                    gps_status = "GREEN"  # Show green for debug mode
                    gps_data = {'latitude': debug_pos[0], 'longitude': debug_pos[1], 'satellites': '8'}
                else:
                    draw_display("Getting GPS...")
                    gps_pos = get_gps_location()
                    if gps_pos:
                        user_lat, user_lon, pos_source = gps_pos[0], gps_pos[1], "GPS"
                        gps_status = "GREEN"  # GPS signal available
                        gps_data = {'latitude': gps_pos[0], 'longitude': gps_pos[1], 'satellites': '?'}
                    else:
                        draw_display("No GPS signal...")
                        # No fallback - just stay in loading state
                        time.sleep(1)
                        continue  # Try GPS again
                position_fetched = True
                last_update = time.time() - update_interval  # Force immediate aircraft fetch
            
            # Continuously check GPS status (only if not in debug mode)
            if pos_source == "GPS" and not DEBUG_MODE:
                # Try to get fresh GPS data
                fresh_gps = get_gps_location()
                if fresh_gps:
                    gps_status = "GREEN"
                    gps_data = {'latitude': fresh_gps[0], 'longitude': fresh_gps[1], 'satellites': '?'}
                else:
                    gps_status = "RED"
                    gps_data = None
            
            # Update aircraft data periodically
            current_time = time.time()
            if position_fetched and current_time - last_update > update_interval:
                draw_display("Getting aircraft...")
                aircraft_list = get_aircraft(user_lat, user_lon)
                # Always update last_update to prevent infinite requests, even if API failed
                last_update = current_time
                print(f"Found {len(aircraft_list)} aircraft")
            
            # Update display
            draw_display()
            
            time.sleep(0.1)  # Update display at 10 FPS
            
    finally:
        lcd.clear()
        print("Traffic page closed")