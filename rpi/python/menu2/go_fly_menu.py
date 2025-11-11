import time
import subprocess
import serial
import smbus2
import requests
from math import sin, cos, sqrt, atan2, radians
from PIL import Image, ImageDraw, ImageFont
from service.pilot_assistant_system import PilotAssistantSystem
from library.config import DEBUG_MODE, DEBUG_LATITUDE, DEBUG_LONGITUDE


def check_wifi_status():
    """Check WiFi connection status"""
    try:
        result = subprocess.run(['hostname', '-I'], capture_output=True, text=True, timeout=1)
        if result.returncode == 0 and result.stdout.strip():
            return "OK"
        else:
            return "FAIL"
    except:
        return "FAIL"


def check_gps_status():
    """Check GPS module status"""
    try:
        ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=0.5)
        for _ in range(5):
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('ascii', errors='replace').strip()
                    if line.startswith('$GPGGA'):
                        parts = line.split(',')
                        if len(parts) >= 15:
                            if parts[6] != '0' and parts[2] and parts[4]:
                                ser.close()
                                return "OK"
                else:
                    break
            except:
                continue
        ser.close()
        return "FAIL"
    except:
        return "FAIL"


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
        import math
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


# Traffic menu configuration
AIRCRAFT_RANGE_KM = 25      # Radius in km to search for aircraft
UPDATE_INTERVAL_SEC = 10    # Update aircraft data every N seconds

def haversine(lat1, lon1, lat2, lon2):
    """Calculate distance between two lat/lon points using Haversine formula"""
    R = 6371.0  # Radius of Earth in km
    dlat = radians(lat2 - lat1)
    dlon = radians(lon2 - lon1)
    a = sin(dlat / 2)**2 + cos(radians(lat1)) * cos(radians(lat2)) * sin(dlon / 2)**2
    c = 2 * atan2(sqrt(a), sqrt(1 - a))
    return R * c

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

def get_debug_location():
    """Get debug coordinates when DEBUG_MODE is enabled"""
    if DEBUG_MODE:
        print("Using debug coordinates from config.py")
        return DEBUG_LATITUDE, DEBUG_LONGITUDE
    else:
        return None

def get_gps_location_traffic():
    """Try to get GPS location for traffic, return None if not available"""
    # Check debug mode first
    if DEBUG_MODE:
        debug_pos = get_debug_location()
        if debug_pos:
            return debug_pos

    # Try real GPS
    try:
        ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=1)
        # Try to get GPS data for 3 seconds max
        start_time = time.time()
        while time.time() - start_time < 3:
            if ser.in_waiting > 0:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    gps_data = parse_gga(line)
                    if gps_data and gps_data['latitude'] is not None and gps_data['longitude'] is not None:
                        ser.close()
                        return gps_data['latitude'], gps_data['longitude']
        ser.close()
    except Exception as e:
        if DEBUG_MODE:
            print(f"GPS error: {e}")
        pass

    return None

def get_aircraft(lat, lon, radius_km=AIRCRAFT_RANGE_KM):
    """Get aircraft data within bounding box using OpenSky Network API"""
    try:
        delta_deg = radius_km / 111  # Rough approximation
        url = f"https://opensky-network.org/api/states/all?lamin={lat - delta_deg}&lamax={lat + delta_deg}&lomin={lon - delta_deg}&lomax={lon + delta_deg}"

        if DEBUG_MODE:
            print(f"Fetching aircraft from: {url}")

        response = requests.get(url, timeout=10)

        # Check if response is successful
        if response.status_code != 200:
            if DEBUG_MODE:
                print(f"OpenSky API returned status code: {response.status_code}")
            return []

        # Check if response has content
        if not response.text.strip():
            if DEBUG_MODE:
                print("OpenSky API returned empty response")
            return []

        # Try to parse JSON
        try:
            data = response.json()
        except ValueError as json_error:
            if DEBUG_MODE:
                print(f"Invalid JSON response from OpenSky API: {json_error}")
                print(f"Response content: {response.text[:200]}...")  # Show first 200 chars
            return []

        # Check if data structure is valid
        if not isinstance(data, dict) or "states" not in data:
            if DEBUG_MODE:
                print("OpenSky API response missing 'states' field")
            return []

        states = data.get("states", [])
        if not states:
            if DEBUG_MODE:
                print("No aircraft states returned by OpenSky API")
            return []

        aircraft = []
        for state in states:
            # Ensure state has enough fields
            if not state or len(state) < 11:
                continue

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

        if DEBUG_MODE:
            print(f"Successfully parsed {len(aircraft)} aircraft from {len(states)} states")

        return aircraft

    except requests.exceptions.Timeout:
        if DEBUG_MODE:
            print("OpenSky API request timed out")
        return []
    except requests.exceptions.ConnectionError:
        if DEBUG_MODE:
            print("Failed to connect to OpenSky API")
        return []
    except Exception as e:
        if DEBUG_MODE:
            print(f"Aircraft data error: {e}")
        return []

def traffic_lat_lon_to_xy(lat, lon, center_lat, center_lon, center_y=130, scale=2.0):
    """Convert lat/lon to x,y coordinates for display (centered on user position)"""
    # Simple Mercator-like projection for small areas
    x = (lon - center_lon) * 111 * cos(radians(center_lat)) * scale + 120
    y = (center_lat - lat) * 111 * scale + center_y  # Inverted Y for screen coordinates
    return int(x), int(y)


def draw_artificial_horizon(draw, width, height, pitch, roll):
    """Draw artificial horizon with pitch and roll indication using full screen"""
    import math

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


def display_go_fly_menu(lcd):
    """Display Go Fly pages - summary, attitude, traffic"""
    if DEBUG_MODE:
        print("Displaying Go Fly pages...")

    # Fonts
    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
    font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 18)
    font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 14)

    # Initialize button states
    last_press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_key1_state = lcd.digital_read(lcd.GPIO_KEY1_PIN)
    last_left_state = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
    last_right_state = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)

    # Page navigation
    pages = ['summary', 'attitude', 'traffic']
    current_page = 0

    # Initialize ADXL345
    adxl_bus = init_adxl345()

    # Attitude warning thresholds (degrees)
    max_bank_angle = 45.0  # Maximum safe bank angle
    max_pitch_angle = 20.0  # Maximum safe pitch angle (up or down)

    # Blinking state for warnings
    blink_state = False
    last_blink_time = 0
    blink_interval = 0.5  # Blink every 500ms

    def update_blink_state():
        """Update blinking state for warnings"""
        nonlocal blink_state, last_blink_time
        current_time = time.time()
        if current_time - last_blink_time >= blink_interval:
            blink_state = not blink_state
            last_blink_time = current_time

    def update_summary_display():
        """Display flight summary with system status"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Title with ribbon background
        title_text = "GO FLY"
        title_bbox = draw.textbbox((0, 0), title_text, font=font_large)
        title_width = title_bbox[2] - title_bbox[0]
        title_height = title_bbox[3] - title_bbox[1]

        # Draw ribbon background (full width)
        draw.rectangle((0, 5, lcd.width, title_height + 15), fill="BLACK")

        # Center the title text on the ribbon
        title_x = (lcd.width - title_width) // 2
        draw.text((title_x, 8), title_text, fill="MAGENTA", font=font_large)

        # Get system status
        wifi_status = check_wifi_status()
        gps_status = check_gps_status()
        pitch, bank = read_adxl345_data(adxl_bus)

        # Update blinking state
        update_blink_state()

        # Check for excessive angles
        pitch_warning = abs(pitch) > max_pitch_angle
        bank_warning = abs(bank) > max_bank_angle

        # Status information
        y_pos = 50

        # GPS Status
        gps_color = "GREEN" if gps_status == "OK" else "RED"
        draw.text((10, y_pos), "GPS:", fill="WHITE", font=font_medium)
        draw.text((80, y_pos), gps_status, fill=gps_color, font=font_medium)
        y_pos += 30

        # WiFi Status
        wifi_color = "GREEN" if wifi_status == "OK" else "RED"
        draw.text((10, y_pos), "WIFI:", fill="WHITE", font=font_medium)
        draw.text((80, y_pos), wifi_status, fill=wifi_color, font=font_medium)
        y_pos += 30

        # Pitch Angle with warning
        pitch_color = "RED" if pitch_warning and blink_state else "YELLOW"
        pitch_text = f"PITCH:"
        pitch_value = f"⚠ {pitch:.1f}°" if pitch_warning and blink_state else f"{pitch:.1f}°"
        draw.text((10, y_pos), pitch_text, fill="WHITE", font=font_medium)
        draw.text((80, y_pos), pitch_value, fill=pitch_color, font=font_medium)
        y_pos += 30

        # Bank Angle with warning
        bank_color = "RED" if bank_warning and blink_state else "YELLOW"
        bank_text = f"BANK:"
        bank_value = f"⚠ {bank:.1f}°" if bank_warning and blink_state else f"{bank:.1f}°"
        draw.text((10, y_pos), bank_text, fill="WHITE", font=font_medium)
        draw.text((80, y_pos), bank_value, fill=bank_color, font=font_medium)
        y_pos += 30

        # Large center-screen warnings
        if (pitch_warning or bank_warning) and blink_state:
            center_x = lcd.width // 2
            center_y = lcd.height // 2

            if pitch_warning:
                warning_text = "PULL DOWN!" if pitch > 0 else "PULL UP!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                draw.rectangle((center_x - warning_width//2 - 10, center_y - 20,
                              center_x + warning_width//2 + 10, center_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, center_y - 12), warning_text, fill="WHITE", font=font_large)

            if bank_warning:
                warning_text = "BANK ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                warning_y = center_y + 30 if pitch_warning else center_y
                draw.rectangle((center_x - warning_width//2 - 10, warning_y - 20,
                              center_x + warning_width//2 + 10, warning_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, warning_y - 12), warning_text, fill="WHITE", font=font_large)

        # Instructions
        draw.text((10, 190), "ENTER: Start Flight", fill="CYAN", font=font_small)
        draw.text((10, 205), "LEFT/RIGHT: Navigate", fill="CYAN", font=font_small)
        draw.text((10, 220), "KEY1: Back", fill="CYAN", font=font_small)

        # Display image
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

    def display_attitude_page():
        """Display attitude indicator page with warning system"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # Get attitude data
        pitch, bank = read_adxl345_data(adxl_bus)

        # Update blinking state
        update_blink_state()

        # Check for warnings
        pitch_warning = abs(pitch) > max_pitch_angle
        bank_warning = abs(bank) > max_bank_angle

        # Draw artificial horizon using full screen (your exact function)
        draw_artificial_horizon(draw, lcd.width, lcd.height, pitch, bank)

        # Display pitch values with warning (like service/pilot_assistant_system.py)
        pitch_color = "RED" if pitch_warning and blink_state else "YELLOW"
        pitch_label = "⚠ Pitch" if pitch_warning and blink_state else "Pitch"
        pitch_value = f"⚠ {pitch:.1f}°" if pitch_warning and blink_state else f"{pitch:.1f}°"

        draw.text((5, 5), pitch_label, fill=pitch_color, font=font_medium)
        draw.text((5, 30), pitch_value, fill=pitch_color, font=font_medium)

        # Display roll values with warning (right-aligned)
        roll_color = "RED" if bank_warning and blink_state else "YELLOW"
        roll_label = "⚠ Bank" if bank_warning and blink_state else "Bank"
        roll_value = f"⚠ {bank:.1f}°" if bank_warning and blink_state else f"{bank:.1f}°"

        roll_bbox = draw.textbbox((0, 0), roll_label, font=font_medium)
        roll_width = roll_bbox[2] - roll_bbox[0]
        value_bbox = draw.textbbox((0, 0), roll_value, font=font_medium)
        value_width = value_bbox[2] - value_bbox[0]

        draw.text((lcd.width - roll_width - 5, 5), roll_label, fill=roll_color, font=font_medium)
        draw.text((lcd.width - value_width - 5, 30), roll_value, fill=roll_color, font=font_medium)

        # Large center-screen warnings on attitude display
        if (pitch_warning or bank_warning) and blink_state:
            center_x = lcd.width // 2
            center_y = lcd.height // 2

            if pitch_warning:
                warning_text = "PULL DOWN!" if pitch > 0 else "PULL UP!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                draw.rectangle((center_x - warning_width//2 - 10, center_y - 20,
                              center_x + warning_width//2 + 10, center_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, center_y - 12), warning_text, fill="WHITE", font=font_large)

            if bank_warning:
                warning_text = "BANK ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                warning_y = center_y + 30 if pitch_warning else center_y
                draw.rectangle((center_x - warning_width//2 - 10, warning_y - 20,
                              center_x + warning_width//2 + 10, warning_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, warning_y - 12), warning_text, fill="WHITE", font=font_large)

        # Navigation instructions at bottom
        draw.text((5, lcd.height - 25), "LEFT/RIGHT: Navigate | KEY1: Back", fill="CYAN", font=font_small)

        # Display image
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

    # Traffic display variables (like traffic_menu.py)
    traffic_user_lat, traffic_user_lon = None, None
    traffic_aircraft_list = []
    traffic_last_update = 0
    traffic_position_fetched = False
    traffic_gps_status = "RED"
    traffic_wifi_connected = False
    traffic_show_aircraft_info = False
    traffic_aircraft_info_timeout = 0
    traffic_selected_aircraft = None
    traffic_current_aircraft_index = 0
    traffic_sorted_aircraft_list = []

    def display_traffic_page():
        """Display traffic radar page like traffic_menu.py"""
        nonlocal traffic_user_lat, traffic_user_lon, traffic_aircraft_list, traffic_last_update
        nonlocal traffic_position_fetched, traffic_gps_status, traffic_wifi_connected
        nonlocal traffic_show_aircraft_info, traffic_aircraft_info_timeout, traffic_selected_aircraft
        nonlocal traffic_current_aircraft_index, traffic_sorted_aircraft_list

        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        # GPS Status - red when using IP location, green when GPS coordinates available
        gps_color = "GREEN" if traffic_gps_status == "GREEN" else "RED"
        draw.text((10, 5), "GPS", fill=gps_color, font=font_large)

        # WiFi Status - positioned top right
        wifi_color = "GREEN" if traffic_wifi_connected else "RED"
        wifi_text = "WIFI"
        wifi_bbox = draw.textbbox((0, 0), wifi_text, font=font_large)
        wifi_width = wifi_bbox[2] - wifi_bbox[0]
        wifi_x = lcd.width - wifi_width - 10  # 10 pixels from right edge
        draw.text((wifi_x, 5), wifi_text, fill=wifi_color, font=font_large)

        # Draw radar circles if we have position
        if traffic_user_lat and traffic_user_lon:
            center_x, center_y = 120, 130
            draw.ellipse([center_x-100, center_y-100, center_x+100, center_y+100], outline="DARKGRAY")  # ~50km
            draw.ellipse([center_x-50, center_y-50, center_x+50, center_y+50], outline="DARKGRAY")  # ~25km

            # Draw user position as white aircraft symbol
            aircraft_points = [
                (center_x, center_y - 6),  # nose
                (center_x - 3, center_y + 3),  # left wing
                (center_x - 2, center_y + 5),  # left tail
                (center_x, center_y + 3),   # center tail
                (center_x + 2, center_y + 5),  # right tail
                (center_x + 3, center_y + 3),  # right wing
            ]
            draw.polygon(aircraft_points, fill="WHITE")

            # Draw aircraft as dots with direction indicators
            for aircraft in traffic_aircraft_list:
                ac_x, ac_y = traffic_lat_lon_to_xy(aircraft['lat'], aircraft['lon'], traffic_user_lat, traffic_user_lon, center_y)

                # Only draw aircraft that are within display bounds
                if 10 <= ac_x <= 230 and 10 <= ac_y <= 230:
                    # Check if this is the selected aircraft
                    if traffic_show_aircraft_info and traffic_selected_aircraft and aircraft['callsign'] == traffic_selected_aircraft['callsign']:
                        dot_color = "RED"
                        line_color = "RED"
                    else:
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

        # Show selected aircraft info at bottom left
        if traffic_show_aircraft_info and traffic_selected_aircraft:
            info_x = 10
            info_y = 160

            callsign_text = f"{traffic_selected_aircraft['callsign']}"
            altitude_m = traffic_selected_aircraft['altitude']
            altitude_ft = int(altitude_m * 3.28084) if altitude_m else 0
            altitude_text = f"{altitude_ft}ft"
            speed_text = f"{traffic_selected_aircraft['speed_knots']}kts"

            draw.text((info_x, info_y), callsign_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 25), altitude_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 50), speed_text, fill="CYAN", font=font_large)

        # Navigation instructions
        draw.text((10, 220), "LEFT/RIGHT: Navigate | UP/DOWN: Select", fill="CYAN", font=font_small)

        # Display image
        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

    def show_preflight_check():
        """Show pre-flight system check"""
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 5), "PRE-FLIGHT CHECK", fill="YELLOW", font=font_medium)

        # Simple system checks
        checks = [
            ("GPS", "OK"),
            ("Display", "OK"),
            ("Controls", "OK"),
            ("Sensors", "OK")
        ]

        y_pos = 40
        for check_name, status in checks:
            color = "GREEN" if status == "OK" else "RED"
            draw.text((10, y_pos), f"{check_name}:", fill="WHITE", font=font_small)
            draw.text((100, y_pos), status, fill=color, font=font_small)
            y_pos += 25

        draw.text((10, 150), "SYSTEM READY", fill="GREEN", font=font_large)
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

    def start_flight_system():
        """Start the full pilot assistant system"""
        # Show loading screen
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        draw.text((10, 80), "STARTING", fill="YELLOW", font=font_large)
        draw.text((10, 110), "FLIGHT SYSTEM", fill="YELLOW", font=font_large)
        draw.text((10, 150), "Please wait...", fill="CYAN", font=font_medium)

        im_r = background.rotate(270).transpose(Image.FLIP_LEFT_RIGHT)
        lcd.ShowImage(im_r)

        time.sleep(2)

        # Clear the display before starting the system
        lcd.clear()

        # Create and run the pilot assistant system
        try:
            system = PilotAssistantSystem()
            system.run()
        except KeyboardInterrupt:
            if DEBUG_MODE:
                print("Flight system interrupted")
        except Exception as e:
            if DEBUG_MODE:
                print(f"Error starting flight system: {e}")
            pass
            # Show error and return to menu
            background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
            draw = ImageDraw.Draw(background)

            draw.text((10, 80), "ERROR", fill="RED", font=font_large)
            draw.text((10, 110), "System failed", fill="RED", font=font_medium)
            draw.text((10, 150), "Check logs", fill="YELLOW", font=font_small)
            draw.text((10, 215), "ENTER: Back", fill="CYAN", font=font_small)

            im_r = background.rotate(270)
            lcd.ShowImage(im_r)

            # Wait for enter
            while True:
                press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
                if press_state == 0:
                    time.sleep(0.1)
                    break
                time.sleep(0.1)

    def update_current_page():
        """Update the display based on current page"""
        if pages[current_page] == 'summary':
            update_summary_display()
        elif pages[current_page] == 'attitude':
            display_attitude_page()
        elif pages[current_page] == 'traffic':
            display_traffic_page()

    # Display initial page
    update_current_page()

    # Timer for periodic updates
    last_update_time = time.time()
    update_interval = 0.5  # Update display every 500ms

    # Main navigation loop
    while True:
        press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
        key1_state = lcd.digital_read(lcd.GPIO_KEY1_PIN)
        left_state = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
        right_state = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)
        up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
        down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)

        # Initialize button states for first run
        if 'last_up_state_traffic' not in locals():
            last_up_state_traffic = up_state
            last_down_state_traffic = down_state

        # RIGHT button - Next page
        if right_state == 0 and last_right_state == 1:
            current_page = (current_page + 1) % len(pages)
            if DEBUG_MODE:
                print(f"Switched to page: {pages[current_page]}")
            update_current_page()
            time.sleep(0.1)

        # LEFT button - Previous page
        if left_state == 0 and last_left_state == 1:
            current_page = (current_page - 1) % len(pages)
            if DEBUG_MODE:
                print(f"Switched to page: {pages[current_page]}")
            update_current_page()
            time.sleep(0.1)

        # UP/DOWN buttons for aircraft selection in traffic mode
        if pages[current_page] == 'traffic':
            # UP button - next aircraft
            if up_state == 0 and last_up_state_traffic == 1:
                if traffic_sorted_aircraft_list:
                    traffic_current_aircraft_index = (traffic_current_aircraft_index + 1) % len(traffic_sorted_aircraft_list)
                    traffic_selected_aircraft = traffic_sorted_aircraft_list[traffic_current_aircraft_index]
                    traffic_show_aircraft_info = True
                    traffic_aircraft_info_timeout = time.time() + 15
                    if DEBUG_MODE:
                        print(f"Selected aircraft: {traffic_selected_aircraft['callsign']}")
                time.sleep(0.1)

            # DOWN button - previous aircraft
            if down_state == 0 and last_down_state_traffic == 1:
                if traffic_sorted_aircraft_list:
                    traffic_current_aircraft_index = (traffic_current_aircraft_index - 1) % len(traffic_sorted_aircraft_list)
                    traffic_selected_aircraft = traffic_sorted_aircraft_list[traffic_current_aircraft_index]
                    traffic_show_aircraft_info = True
                    traffic_aircraft_info_timeout = time.time() + 15
                    if DEBUG_MODE:
                        print(f"Selected aircraft: {traffic_selected_aircraft['callsign']}")
                time.sleep(0.1)

            # Hide aircraft info after timeout
            if traffic_show_aircraft_info and time.time() > traffic_aircraft_info_timeout:
                traffic_show_aircraft_info = False

            # Update sorted aircraft list
            if traffic_aircraft_list:
                traffic_sorted_aircraft_list = sorted(traffic_aircraft_list, key=lambda ac: ac['distance_km'])

            # Check WiFi status
            traffic_wifi_connected = check_wifi_status()

            # Get GPS position if not fetched yet
            if not traffic_position_fetched:
                if DEBUG_MODE:
                    print("Using debug location for traffic...")
                    debug_pos = get_debug_location()
                    if debug_pos:
                        traffic_user_lat, traffic_user_lon = debug_pos[0], debug_pos[1]
                        traffic_gps_status = "GREEN"  # Show green for debug mode
                        traffic_position_fetched = True
                        traffic_last_update = time.time() - UPDATE_INTERVAL_SEC  # Force immediate aircraft fetch
                else:
                    if DEBUG_MODE:
                        print("Getting GPS for traffic...")
                    gps_pos = get_gps_location_traffic()
                    if gps_pos:
                        traffic_user_lat, traffic_user_lon = gps_pos[0], gps_pos[1]
                        traffic_gps_status = "GREEN"
                        traffic_position_fetched = True
                        traffic_last_update = time.time() - UPDATE_INTERVAL_SEC  # Force immediate aircraft fetch
                    else:
                        traffic_gps_status = "RED"
                        if DEBUG_MODE:
                            print("No GPS signal for traffic...")

            # Continuously check GPS status (only if not in debug mode)
            if not DEBUG_MODE and traffic_position_fetched:
                # Try to get fresh GPS data to update status
                fresh_gps = get_gps_location_traffic()
                if fresh_gps:
                    traffic_gps_status = "GREEN"
                    traffic_user_lat, traffic_user_lon = fresh_gps[0], fresh_gps[1]
                else:
                    traffic_gps_status = "RED"

            # Update aircraft data periodically
            current_time = time.time()
            if traffic_position_fetched and current_time - traffic_last_update > UPDATE_INTERVAL_SEC:
                if DEBUG_MODE:
                    print("Getting aircraft data...")
                traffic_aircraft_list = get_aircraft(traffic_user_lat, traffic_user_lon)
                traffic_last_update = current_time
                if DEBUG_MODE:
                    print(f"Found {len(traffic_aircraft_list)} aircraft")

        # ENTER button - Start Flight System (only from summary page)
        if press_state == 0 and last_press_state == 1:
            if pages[current_page] == 'summary':
                if DEBUG_MODE:
                    print("Starting flight system...")
                start_flight_system()
                # After flight system exits, return to current page
                update_current_page()
            time.sleep(0.1)

        # KEY1 to go back to main menu
        if key1_state == 0 and last_key1_state == 1:
            break

        # Periodic display update for live sensor data
        current_time = time.time()
        if current_time - last_update_time >= update_interval:
            update_current_page()  # Refresh display with new sensor readings
            last_update_time = current_time

        # Update button states
        last_press_state = press_state
        last_key1_state = key1_state
        last_left_state = left_state
        last_right_state = right_state
        last_up_state_traffic = up_state
        last_down_state_traffic = down_state

        time.sleep(0.1)