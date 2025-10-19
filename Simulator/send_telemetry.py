import serial
import json
import time
import requests
from math import sin, cos, sqrt, atan2, radians

# === CONFIGURATION ===
SERIAL_PORT = "/dev/cu.usbmodem14101"  # update for your system
BAUDRATE = 115200
UPDATE_RATE_HZ = 1  # how often to send to Pico
COPENHAGEN_LAT, COPENHAGEN_LON = 55.6180, 12.6560  # EKCH center point
AIRCRAFT_RANGE_KM = 25  # Radius in km to search for aircraft

# === INITIALIZE SERIAL ===
ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
print(f"Connected to {SERIAL_PORT} at {BAUDRATE} baud.")

# === HELPER FUNCTIONS ===
def haversine(lat1, lon1, lat2, lon2):
    """Calculate distance between two lat/lon points using Haversine formula"""
    R = 6371.0  # Radius of Earth in km
    dlat = radians(lat2 - lat1)
    dlon = radians(lon2 - lon1)
    a = sin(dlat / 2)**2 + cos(radians(lat1)) * cos(radians(lat2)) * sin(dlon / 2)**2
    c = 2 * atan2(sqrt(a), sqrt(1 - a))
    return R * c

def fetch_aircraft_data(lat, lon, radius_km=AIRCRAFT_RANGE_KM):
    """Retrieve list of aircraft from OpenSky Network API."""
    try:
        delta_deg = radius_km / 111  # Rough approximation: 1 degree ≈ 111 km
        url = f"https://opensky-network.org/api/states/all?lamin={lat - delta_deg}&lamax={lat + delta_deg}&lomin={lon - delta_deg}&lomax={lon + delta_deg}"
        response = requests.get(url, timeout=10)

        # Check if response has content
        if not response.text.strip():
            print("OpenSky API returned empty response")
            return []

        try:
            data = response.json()
        except ValueError as json_error:
            print(f"OpenSky API returned invalid JSON: {json_error}")
            return []

        aircraft = []
        states = data.get("states", [])
        if not states:
            print("OpenSky API returned no aircraft states")
            return []

        for state in states:
            # Ensure state has enough elements
            if len(state) < 11:
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
        return aircraft
    except requests.RequestException as e:
        print(f"Network error fetching aircraft data: {e}")
        return []
    except Exception as e:
        print(f"Unexpected error fetching aircraft data: {e}")
        return []

# === MAIN LOOP ===
interval = 1.0 / UPDATE_RATE_HZ

try:
    while True:
        # Get aircraft nearby Copenhagen
        aircraft_list = fetch_aircraft_data(COPENHAGEN_LAT, COPENHAGEN_LON)

        # Simulate ownship (Copenhagen tower position)
        own = {
            "lat": COPENHAGEN_LAT,
            "lon": COPENHAGEN_LON,
            "alt": 0,
            "pitch": 0,
            "roll": 0,
            "yaw": 0  # you can randomize if you want radar rotation
        }

        # Build telemetry traffic list
        traffic_json = []
        for ac in aircraft_list:
            traffic_json.append({
                "id": ac["callsign"],
                "lat": ac["lat"],
                "lon": ac["lon"],
                "alt": ac["altitude"]
            })

        # Complete telemetry JSON structure
        telemetry = {
            "own": own,
            "traffic": traffic_json,
            "status": {
                "wifi": True,
                "gps": True,
                "bluetooth": True
            }
        }

        # Send to Pico via serial
        json_str = json.dumps(telemetry) + "\n"
        ser.write(json_str.encode('utf-8'))
        print(f"Sent {len(traffic_json)} aircraft around EKCH (Copenhagen)")

        time.sleep(interval)

except KeyboardInterrupt:
    print("\nStopping telemetry stream.")
finally:
    ser.close()
