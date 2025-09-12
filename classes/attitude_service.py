import threading
import time
import smbus2
from math import atan2, degrees


class AttitudeService:
    """Background attitude service that reads accelerometer/gyro data"""
    def __init__(self):
        self.running = False
        self.pitch = 0.0  # Pitch angle in degrees (+ = nose up, - = nose down)
        self.roll = 0.0   # Roll angle in degrees (+ = right wing down, - = left wing down)
        self.data_lock = threading.Lock()
        self.bus = None
        self.adxl345_addr = 0x53  # ADXL345 I2C address
        
        # Calibration offsets (can be adjusted)
        self.pitch_offset = 0.0
        self.roll_offset = 0.0
    
    def init_adxl345(self):
        """Initialize ADXL345 accelerometer"""
        try:
            self.bus = smbus2.SMBus(1)  # I2C bus 1
            
            # Wake up the ADXL345 (same as gyro_menu)
            self.bus.write_byte_data(self.adxl345_addr, 0x2D, 0x08)
            
            # Set data format (±2g, full resolution, same as gyro_menu)
            self.bus.write_byte_data(self.adxl345_addr, 0x31, 0x08)
            
            print("ADXL345 initialized successfully")
            return True
        except Exception as e:
            print(f"Error initializing ADXL345: {e}")
            return False
    
    def read_acceleration(self):
        """Read acceleration data from ADXL345 (same method as gyro_menu)"""
        if not self.bus:
            return None, None, None
        
        try:
            # Read 6 bytes from DATAX0 to DATAZ1
            data = self.bus.read_i2c_block_data(self.adxl345_addr, 0x32, 6)
            
            # Convert to signed 16-bit integers (same as gyro_menu)
            x = int.from_bytes(data[0:2], byteorder='little', signed=True) / 256.0
            y = int.from_bytes(data[2:4], byteorder='little', signed=True) / 256.0
            z = int.from_bytes(data[4:6], byteorder='little', signed=True) / 256.0
            
            return x, y, z
        except Exception as e:
            print(f"Error reading accelerometer: {e}")
            return None, None, None
    
    def calculate_attitude(self, x, y, z):
        """Calculate pitch and roll from accelerometer data (same as gyro_menu)"""
        try:
            # Calculate pitch and roll in degrees (same formulas as gyro_menu)
            pitch = degrees(atan2(-y, z))  # Pitch uses only Y and Z axes
            roll = degrees(atan2(x, z))   # Roll uses only X and Z axes
            
            # Apply calibration offsets
            pitch -= self.pitch_offset
            roll -= self.roll_offset
            
            return pitch, roll
        except Exception as e:
            print(f"Error calculating attitude: {e}")
            return 0.0, 0.0
    
    def start(self):
        """Start the attitude service"""
        self.running = True
        if self.init_adxl345():
            attitude_thread = threading.Thread(target=self._attitude_worker, daemon=True)
            attitude_thread.start()
            print("Attitude service started")
        else:
            print("Failed to start attitude service - ADXL345 not available")
            self.running = False
    
    def _attitude_worker(self):
        """Background thread that reads attitude data"""
        while self.running:
            try:
                x, y, z = self.read_acceleration()
                if x is not None:
                    pitch, roll = self.calculate_attitude(x, y, z)
                    
                    with self.data_lock:
                        self.pitch = pitch
                        self.roll = roll
            except Exception as e:
                print(f"Attitude service error: {e}")
            
            time.sleep(0.05)  # 20 Hz update rate
    
    def get_attitude(self):
        """Get current attitude data (thread-safe)"""
        with self.data_lock:
            return {
                'pitch': self.pitch,
                'roll': self.roll
            }
    
    def calibrate(self, pitch_offset=0.0, roll_offset=0.0):
        """Set calibration offsets"""
        self.pitch_offset = pitch_offset
        self.roll_offset = roll_offset
    
    def stop(self):
        """Stop the attitude service"""
        self.running = False
        if self.bus:
            self.bus.close()