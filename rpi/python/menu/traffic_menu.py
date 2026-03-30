import requests
import time
import serial
import subprocess
import json
from math import sin, cos, sqrt, atan2, radians
from PIL import Image, ImageDraw, ImageFont
from gpiozero import DigitalOutputDevice

from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT, DEBUG_MODE, DEBUG_LATITUDE, DEBUG_LONGITUDE

# Traffic menu configuration
AIRCRAFT_RANGE_KM = 25      # Radius in km to search for aircraft (good for Cessna at 100kts)
UPDATE_INTERVAL_SEC = 5     # Update aircraft data every N seconds
TELEMETRY_INTERVAL_SEC = 3  # Send telemetry to Pico every N seconds
MAX_TELEMETRY_AIRCRAFT = 8
PICO_PORT = "/dev/ttyACM0"
PICO_BAUDRATE = 115200
FALLBACK_LATITUDE = 59.6519   # Stockholm Arlanda
FALLBACK_LONGITUDE = 17.9186  # Stockholm Arlanda
GPS_BOOT_TIME_SEC = 0.3
GPS_ACQUIRE_TIMEOUT_SEC = 1.5
GPS_REFRESH_INTERVAL_SEC = 20
OPENSKY_TIMEOUT_SEC = 3
AIRCRAFT_DOT_RADIUS = 9

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
            time.sleep(GPS_BOOT_TIME_SEC)  # Brief GPS boot time
        except Exception as e:
            print(f"Error setting up GPS enable pin: {e}")
            gps_en = None
    
    try:
        ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=1)
        # Try to get GPS data for a short window to keep UI responsive
        start_time = time.time()
        while time.time() - start_time < GPS_ACQUIRE_TIMEOUT_SEC:
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
        response = requests.get(url, timeout=OPENSKY_TIMEOUT_SEC)
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

def open_pico_serial():
    """Open serial link to Pico for telemetry output."""
    try:
        pico_serial = serial.Serial(PICO_PORT, PICO_BAUDRATE, timeout=0)
        print(f"Pico telemetry serial connected: {PICO_PORT}")
        return pico_serial
    except Exception as e:
        print(f"Pico telemetry serial unavailable: {e}")
        return None


def send_telemetry_to_pico(pico_serial, user_lat, user_lon, gps_status, wifi_connected, aircraft_list):
    """Send telemetry JSON to Pico."""
    telemetry_traffic_raw = sorted(aircraft_list, key=lambda ac: ac.get('distance_km', 9999))[:MAX_TELEMETRY_AIRCRAFT]
    # Keep payload compact and aligned with Pico parser fields.
    telemetry_traffic = []
    for ac in telemetry_traffic_raw:
        telemetry_traffic.append(
            {
                "id": ac.get("callsign", "N/A")[:7],
                "lat": round(ac.get("lat", 0.0), 6),
                "lon": round(ac.get("lon", 0.0), 6),
                "alt": round(ac.get("altitude", 0.0), 1),
            }
        )

    payload = {
        "own": {
            "lat": round(user_lat, 6),
            "lon": round(user_lon, 6),
            "alt": 0.0,
            "pitch": 0.0,
            "roll": 0.0,
            "yaw": 0.0,
        },
        "status": {
            "gps": gps_status == "GREEN",
            "wifi": bool(wifi_connected),
            "bluetooth": False,
        },
        "warnings": {
            "bank": False,
            "pitch": False,
        },
        "traffic": telemetry_traffic,
    }

    telemetry_json = json.dumps(payload, separators=(",", ":")) + "\n"
    print(f"Telemetry JSON ({len(telemetry_json)} bytes): {telemetry_json.strip()}")

    if not pico_serial:
        return

    try:
        pico_serial.write(telemetry_json.encode("utf-8"))
        print("Telemetry sent to Pico")
    except Exception as e:
        print(f"Telemetry send error: {e}")


def display_traffic_page(lcd):
    """Display traffic map continuously and send telemetry JSON to Pico."""
    print("Traffic-only display started...")

    font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)

    last_press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
    last_up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    last_down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)

    user_lat, user_lon = FALLBACK_LATITUDE, FALLBACK_LONGITUDE
    pos_source = "FALLBACK"
    gps_status = "RED"
    wifi_connected = False
    aircraft_list = []
    sorted_aircraft_list = []
    selected_aircraft = None
    current_aircraft_index = 0
    show_aircraft_info = False
    aircraft_info_timeout = 0

    last_aircraft_update = 0
    last_wifi_check = 0
    last_gps_refresh = time.time()
    last_telemetry_send = 0

    pico_serial = open_pico_serial()

    def draw_display():
        background = Image.new("RGB", (lcd.width, lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)

        gps_color = "GREEN" if gps_status == "GREEN" else "RED"
        draw.text((10, 5), "GPS", fill=gps_color, font=font_large)

        wifi_color = "GREEN" if wifi_connected else "RED"
        wifi_text = "WIFI"
        wifi_bbox = draw.textbbox((0, 0), wifi_text, font=font_large)
        wifi_width = wifi_bbox[2] - wifi_bbox[0]
        wifi_x = lcd.width - wifi_width - 10
        draw.text((wifi_x, 5), wifi_text, fill=wifi_color, font=font_large)

        center_x, center_y = 120, 130
        draw.ellipse([center_x-100, center_y-100, center_x+100, center_y+100], outline="DARKGRAY")
        draw.ellipse([center_x-50, center_y-50, center_x+50, center_y+50], outline="DARKGRAY")

        own_points = [
            (center_x, center_y - 6),
            (center_x - 3, center_y + 3),
            (center_x - 2, center_y + 5),
            (center_x, center_y + 3),
            (center_x + 2, center_y + 5),
            (center_x + 3, center_y + 3),
        ]
        draw.polygon(own_points, fill="WHITE")

        for aircraft in aircraft_list:
            ac_x, ac_y = lat_lon_to_xy(aircraft['lat'], aircraft['lon'], user_lat, user_lon, center_y)
            if 10 <= ac_x <= 230 and 10 <= ac_y <= 230:
                if show_aircraft_info and selected_aircraft and aircraft['callsign'] == selected_aircraft['callsign']:
                    dot_color = "RED"
                    line_color = "RED"
                else:
                    dot_color = "GREEN"
                    line_color = "GREEN"

                draw.ellipse(
                    [
                        ac_x - AIRCRAFT_DOT_RADIUS,
                        ac_y - AIRCRAFT_DOT_RADIUS,
                        ac_x + AIRCRAFT_DOT_RADIUS,
                        ac_y + AIRCRAFT_DOT_RADIUS,
                    ],
                    fill=dot_color,
                )
                if aircraft['heading'] is not None:
                    line_length = 12
                    end_x = ac_x + line_length * sin(radians(aircraft['heading']))
                    end_y = ac_y - line_length * cos(radians(aircraft['heading']))
                    draw.line([ac_x, ac_y, end_x, end_y], fill=line_color, width=2)

        if show_aircraft_info and selected_aircraft:
            info_x = 10
            info_y = 160
            callsign_text = f"{selected_aircraft['callsign']}"
            altitude_m = selected_aircraft['altitude']
            altitude_ft = int(altitude_m * 3.28084) if altitude_m else 0
            altitude_text = f"{altitude_ft}ft"
            speed_text = f"{selected_aircraft['speed_knots']}kts"
            draw.text((info_x, info_y), callsign_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 25), altitude_text, fill="CYAN", font=font_large)
            draw.text((info_x, info_y + 50), speed_text, fill="CYAN", font=font_large)

        im_r = background.rotate(270)
        lcd.ShowImage(im_r)

    # Initial position load
    if DEBUG_MODE:
        debug_pos = get_debug_location()
        user_lat, user_lon = debug_pos[0], debug_pos[1]
        gps_status = "GREEN"
        pos_source = "DEBUG"
    else:
        gps_pos = get_gps_location()
        if gps_pos:
            user_lat, user_lon = gps_pos[0], gps_pos[1]
            gps_status = "GREEN"
            pos_source = "GPS"
        else:
            user_lat, user_lon = FALLBACK_LATITUDE, FALLBACK_LONGITUDE
            gps_status = "RED"
            pos_source = "FALLBACK"
            print("No GPS signal, using Arlanda fallback coordinates.")

    draw_display()
    time.sleep(0.2)

    try:
        while True:
            now = time.time()

            press_state = lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN)
            if press_state == 0 and last_press_state == 1:
                time.sleep(0.05)
                if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
                    break
            last_press_state = press_state

            if aircraft_list:
                sorted_aircraft_list = sorted(aircraft_list, key=lambda ac: ac['distance_km'])

            up_state = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
            if up_state == 0 and last_up_state == 1 and sorted_aircraft_list:
                current_aircraft_index = (current_aircraft_index + 1) % len(sorted_aircraft_list)
                selected_aircraft = sorted_aircraft_list[current_aircraft_index]
                show_aircraft_info = True
                aircraft_info_timeout = now + 15
            last_up_state = up_state

            down_state = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
            if down_state == 0 and last_down_state == 1 and sorted_aircraft_list:
                current_aircraft_index = (current_aircraft_index - 1) % len(sorted_aircraft_list)
                selected_aircraft = sorted_aircraft_list[current_aircraft_index]
                show_aircraft_info = True
                aircraft_info_timeout = now + 15
            last_down_state = down_state

            if show_aircraft_info and now > aircraft_info_timeout:
                show_aircraft_info = False

            if now - last_wifi_check >= 2:
                wifi_connected = check_wifi_status()
                last_wifi_check = now

            # Send telemetry before potentially blocking network/GPS refreshes
            if now - last_telemetry_send >= TELEMETRY_INTERVAL_SEC:
                send_telemetry_to_pico(
                    pico_serial=pico_serial,
                    user_lat=user_lat,
                    user_lon=user_lon,
                    gps_status=gps_status,
                    wifi_connected=wifi_connected,
                    aircraft_list=aircraft_list,
                )
                last_telemetry_send = now

            if not DEBUG_MODE and now - last_gps_refresh >= GPS_REFRESH_INTERVAL_SEC:
                fresh_gps = get_gps_location()
                if fresh_gps:
                    user_lat, user_lon = fresh_gps[0], fresh_gps[1]
                    gps_status = "GREEN"
                    pos_source = "GPS"
                else:
                    user_lat, user_lon = FALLBACK_LATITUDE, FALLBACK_LONGITUDE
                    gps_status = "RED"
                    pos_source = "FALLBACK"
                last_gps_refresh = now

            if now - last_aircraft_update >= UPDATE_INTERVAL_SEC:
                aircraft_list = get_aircraft(user_lat, user_lon)
                last_aircraft_update = now
                print(f"Traffic update ({pos_source}): {len(aircraft_list)} aircraft")

            draw_display()
            time.sleep(0.1)

    finally:
        if pico_serial:
            try:
                pico_serial.close()
            except Exception:
                pass
        lcd.clear()
        print("Traffic page closed")
