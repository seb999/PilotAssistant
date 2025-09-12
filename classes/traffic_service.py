import threading
import time
import requests
from math import sin, cos, sqrt, atan2, radians


class TrafficService:
    """Background traffic service that fetches aircraft data"""
    def __init__(self, gps_service=None):
        self.running = False
        self.aircraft_list = []
        self.last_update = 0
        self.update_interval = 10  # Update every 10 seconds
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
    
    def get_aircraft(self, lat, lon, radius_km=40):
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
        except Exception as e:
            print(f"Aircraft data error: {e}")
            return []
    
    def start(self):
        """Start the traffic service"""
        self.running = True
        traffic_thread = threading.Thread(target=self._traffic_worker, daemon=True)
        traffic_thread.start()
        print("Traffic service started")
    
    def _traffic_worker(self):
        """Background thread that fetches aircraft data"""
        while self.running:
            try:
                current_time = time.time()
                if current_time - self.last_update > self.update_interval:
                    user_lat = user_lon = None
                    
                    # Try to get GPS location first
                    if self.gps_service:
                        gps_data = self.gps_service.get_data()
                        if (gps_data['gps_data'] and 
                            gps_data['gps_data']['latitude'] and 
                            gps_data['gps_data']['longitude']):
                            user_lat = gps_data['gps_data']['latitude']
                            user_lon = gps_data['gps_data']['longitude']
                            print(f"Traffic service: Using GPS location {user_lat:.4f}, {user_lon:.4f}")
                    
                    # Fallback to IP geolocation if no GPS
                    if not user_lat or not user_lon:
                        user_lat, user_lon = self.get_fallback_location()
                        print(f"Traffic service: Using IP location {user_lat:.4f}, {user_lon:.4f}")
                    
                    if user_lat and user_lon:
                        aircraft = self.get_aircraft(user_lat, user_lon)
                        with self.data_lock:
                            self.aircraft_list = aircraft
                        print(f"Traffic service: Found {len(aircraft)} aircraft")
                        self.last_update = current_time
            except Exception as e:
                print(f"Traffic service error: {e}")
            
            time.sleep(1)  # Check every second
    
    def get_fallback_location(self):
        """Get location from IP geolocation as fallback"""
        try:
            response = requests.get("https://ipinfo.io/json", timeout=5)
            data = response.json()
            if 'loc' in data:
                lat_str, lon_str = data['loc'].split(',')
                return float(lat_str), float(lon_str)
        except Exception as e:
            print(f"IP geolocation error: {e}")
            # Return default coordinates (Berlin) if all else fails
            return 52.5200, 13.4050
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