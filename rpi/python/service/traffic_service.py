import threading
import time
import requests
import sys
import os
from math import sin, cos, sqrt, atan2, radians

# Import debug configuration
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library.config import DEBUG_MODE, DEBUG_LATITUDE, DEBUG_LONGITUDE

# Traffic service configuration
AIRCRAFT_RANGE_KM = 25      # Radius in km to search for aircraft (good for Cessna at 100kts)
UPDATE_INTERVAL_SEC = 30   # Update aircraft data every N seconds


class TrafficService:
    """Background traffic service that fetches aircraft data"""
    def __init__(self, gps_service=None):
        self.running = False
        self.aircraft_list = []
        self.last_update = 0
        self.update_interval = 30
        self.data_lock = threading.Lock()
        self.gps_service = gps_service
    
    def haversine(self, lat1, lon1, lat2, lon2):
        """Calculate distance between two lat/lon points using Haversine formula"""
        R = 6371.0  # Radius of Earth in km
        dlat = radians(lat2 - lat1)
        dlon = radians(lon2 - lon1)
        a = sin(dlat / 2)**2 + cos(radians(lat1)) * cos(radians(lat2)) * sin(dlon / 2)**2
        c = 2 * atan2(sqrt(a), sqrt(1 - a))
        return R * c
    
    def get_aircraft(self, lat, lon, radius_km=AIRCRAFT_RANGE_KM):
        """Get aircraft data within bounding box using OpenSky Network API"""
        try:
            delta_deg = radius_km / 111  # Rough approximation
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
                print(f"Response content: {response.text[:100]}...")  # First 100 chars for debugging
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
                    dist = self.haversine(lat, lon, ac_lat, ac_lon)
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
    
    def start(self):
        """Start the traffic service"""
        self.running = True
        traffic_thread = threading.Thread(target=self._traffic_worker, daemon=True)
        traffic_thread.start()
        print("Traffic service started")
    
    def _traffic_worker(self):
        """Background thread that fetches aircraft data"""
        position_fetched = False
        user_lat = user_lon = None
        
        while self.running:
            try:
                # Get position first (only once)
                if not position_fetched:
                    # Try to get GPS location first
                    if self.gps_service:
                        gps_data = self.gps_service.get_data()
                        if (gps_data['gps_data'] and 
                            gps_data['gps_data']['latitude'] and 
                            gps_data['gps_data']['longitude']):
                            user_lat = gps_data['gps_data']['latitude']
                            user_lon = gps_data['gps_data']['longitude']
                            print(f"Traffic service: Using GPS location {user_lat:.4f}, {user_lon:.4f}")
                            position_fetched = True
                    
                    # Use debug location if no GPS and debug mode enabled
                    if not position_fetched and DEBUG_MODE:
                        user_lat, user_lon = DEBUG_LATITUDE, DEBUG_LONGITUDE
                        print(f"Traffic service: Using debug location {user_lat:.4f}, {user_lon:.4f}")
                        position_fetched = True
                    
                    if not position_fetched:
                        print("Traffic service: No GPS data available, waiting...")
                        time.sleep(2)  # Wait longer when no position
                        continue
                
                # Update aircraft data periodically
                current_time = time.time()
                time_since_last = current_time - self.last_update
                if position_fetched and time_since_last > self.update_interval:
                    print(f"Traffic service: Getting aircraft (last update {time_since_last:.1f}s ago)...")
                    aircraft = self.get_aircraft(user_lat, user_lon)
                    with self.data_lock:
                        self.aircraft_list = aircraft
                    print(f"Traffic service: Found {len(aircraft)} aircraft, next update in {self.update_interval}s")
                    # Always update last_update to prevent infinite requests, even if API failed
                    self.last_update = current_time
                    
            except Exception as e:
                print(f"Traffic service error: {e}")
            
            time.sleep(1)  # Check every second
    
    def get_fallback_location(self):
        """Get debug location if debug mode is enabled"""
        if DEBUG_MODE:
            return DEBUG_LATITUDE, DEBUG_LONGITUDE
        else:
            return None, None
    
    def update_aircraft(self, user_lat, user_lon):
        """Update aircraft list with current user position"""
        if user_lat and user_lon:
            aircraft = self.get_aircraft(user_lat, user_lon)
            with self.data_lock:
                self.aircraft_list = aircraft
    
    def get_data(self):
        """Get current aircraft data (thread-safe)"""
        with self.data_lock:
            return self.aircraft_list.copy()
    
    def stop(self):
        """Stop the traffic service"""
        self.running = False