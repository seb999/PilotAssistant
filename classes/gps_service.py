import threading
import time
import serial
import sys
import os
from gpiozero import DigitalOutputDevice

# Import existing libraries
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library.config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT


class GPSService:
    """Background GPS service that continuously reads GPS data"""
    def __init__(self):
        self.running = False
        self.gps_data = None
        self.gps_status = "RED"
        self.gps_en = None
        self.serial_port = None
        self.data_lock = threading.Lock()
        
    def parse_gga(self, nmea_line):
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
    
    def start(self):
        """Start the GPS service"""
        self.running = True
        
        # Initialize GPS enable pin
        if GPS_EN_PIN is not None:
            try:
                self.gps_en = DigitalOutputDevice(GPS_EN_PIN)
                self.gps_en.on()
                time.sleep(2)  # Wait for GPS to boot
            except Exception as e:
                print(f"Error setting up GPS enable pin: {e}")
        
        # Open GPS serial port
        try:
            self.serial_port = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=GPS_TIMEOUT)
            print(f"GPS service started on {GPS_PORT}")
        except serial.SerialException as e:
            print(f"Error opening GPS port: {e}")
            self.running = False
            return
        
        # Start GPS reading thread
        gps_thread = threading.Thread(target=self._gps_worker, daemon=True)
        gps_thread.start()
    
    def _gps_worker(self):
        """Background thread that reads GPS data"""
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('ascii', errors='replace').strip()
                    if line:
                        gps_data = self.parse_gga(line)
                        if gps_data:
                            with self.data_lock:
                                self.gps_data = gps_data
                                if gps_data['latitude'] is not None and gps_data['longitude'] is not None:
                                    self.gps_status = "GREEN"
                                else:
                                    self.gps_status = "RED"
            except Exception as e:
                print(f"GPS read error: {e}")
            
            time.sleep(0.1)  # Check GPS 10 times per second
    
    def get_data(self):
        """Get current GPS data (thread-safe)"""
        with self.data_lock:
            return {
                'gps_data': self.gps_data,
                'gps_status': self.gps_status
            }
    
    def stop(self):
        """Stop the GPS service"""
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        if self.gps_en:
            self.gps_en.off()
            self.gps_en.close()