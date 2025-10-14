# Raspberry Pi Pico - USB Bulk Transfer GPS Receiver
# This code runs on the Pico to receive GPS data via USB bulk transfers
# Place this file on your Pico and import it in your main.py

import usb_cdc
import struct
import time

class USBGPSReceiver:
    """Receives GPS data from USB bulk transfers"""

    # Binary format must match simulator: latitude(f), longitude(f), satellites(B), fix_quality(B), timestamp(d)
    FORMAT = '<ffBBd'
    SIZE = struct.calcsize(FORMAT)

    def __init__(self):
        self.serial = usb_cdc.data  # USB CDC data interface
        self.latitude = None
        self.longitude = None
        self.satellites = 0
        self.fix_quality = 0
        self.last_update = 0

    def read_binary(self):
        """Read binary GPS data from USB"""
        if self.serial and self.serial.in_waiting >= self.SIZE:
            try:
                data = self.serial.read(self.SIZE)
                if data and len(data) == self.SIZE:
                    # Unpack binary data
                    lat, lon, sats, fix, timestamp = struct.unpack(self.FORMAT, data)

                    self.latitude = lat
                    self.longitude = lon
                    self.satellites = sats
                    self.fix_quality = fix
                    self.last_update = time.ticks_ms()

                    return True
            except Exception as e:
                print(f"Error reading GPS: {e}")
        return False

    def read_nmea(self):
        """Read NMEA GPS data from USB (text mode)"""
        if self.serial and self.serial.in_waiting > 0:
            try:
                line = self.serial.readline()
                if line:
                    line = line.decode('ascii', errors='ignore').strip()
                    if line.startswith('$GPGGA'):
                        # Parse GPGGA sentence
                        parts = line.split(',')
                        if len(parts) >= 15 and parts[6] != '0':
                            # Parse latitude
                            if parts[2] and parts[3]:
                                lat_raw = float(parts[2])
                                lat_deg = int(lat_raw / 100)
                                lat_min = lat_raw - (lat_deg * 100)
                                self.latitude = lat_deg + (lat_min / 60)
                                if parts[3] == 'S':
                                    self.latitude = -self.latitude

                            # Parse longitude
                            if parts[4] and parts[5]:
                                lon_raw = float(parts[4])
                                lon_deg = int(lon_raw / 100)
                                lon_min = lon_raw - (lon_deg * 100)
                                self.longitude = lon_deg + (lon_min / 60)
                                if parts[5] == 'W':
                                    self.longitude = -self.longitude

                            self.satellites = int(parts[7]) if parts[7] else 0
                            self.fix_quality = int(parts[6])
                            self.last_update = time.ticks_ms()

                            return True
            except Exception as e:
                print(f"Error reading NMEA: {e}")
        return False

    def get_data(self):
        """Get current GPS data"""
        return {
            'latitude': self.latitude,
            'longitude': self.longitude,
            'satellites': self.satellites,
            'fix_quality': self.fix_quality,
            'age_ms': time.ticks_diff(time.ticks_ms(), self.last_update) if self.last_update else None
        }

    def has_fix(self):
        """Check if we have a valid GPS fix"""
        return self.latitude is not None and self.longitude is not None and self.fix_quality > 0


# Example usage in your Pico main.py:
"""
from pico_usb_receiver import USBGPSReceiver

gps = USBGPSReceiver()

# For binary mode:
while True:
    if gps.read_binary():
        data = gps.get_data()
        print(f"GPS: {data['latitude']:.6f}, {data['longitude']:.6f}")
        print(f"Satellites: {data['satellites']}, Fix: {data['fix_quality']}")
    time.sleep(0.1)

# For NMEA mode:
while True:
    if gps.read_nmea():
        data = gps.get_data()
        print(f"GPS: {data['latitude']:.6f}, {data['longitude']:.6f}")
    time.sleep(0.1)
"""
