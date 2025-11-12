import serial
import time
import sys
import os
from gpiozero import DigitalOutputDevice

# Add the library directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT

# --- GPS DETECTION AND SETUP ---

print("=== GPS Detection and Raw Data Display ===")
print(f"GPS Port: {GPS_PORT}")
print(f"GPS Baudrate: {GPS_BAUDRATE}")
print(f"GPS Timeout: {GPS_TIMEOUT}")
print(f"GPS Enable Pin: {GPS_EN_PIN}")

gps_en = None
if GPS_EN_PIN is not None:
    try:
        gps_en = DigitalOutputDevice(GPS_EN_PIN)
        gps_en.on()  # Power on the GPS
        print(f"✓ GPS EN pin set to HIGH (GPIO{GPS_EN_PIN})")
        print("Waiting 3 seconds for GPS module to boot...")
        time.sleep(3)  # Give GPS more time to boot
    except Exception as e:
        print(f"⚠ Error setting up GPS enable pin: {e}")
        gps_en = None
else:
    print("⚠ No GPS enable pin configured")

# Open the GPS serial port
try:
    ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=GPS_TIMEOUT)
    print(f"✓ Serial port {GPS_PORT} opened successfully")
    print(f"Listening for GPS data... (Press Ctrl+C to stop)")
    print("=" * 50)
except serial.SerialException as e:
    print(f"✗ Error opening serial port {GPS_PORT}: {e}")
    print("Check if GPS module is connected and port is correct")
    exit(1)

# --- DATA MONITORING ---

line_count = 0
data_detected = False
start_time = time.time()

try:
    while True:
        line = ser.readline().decode('ascii', errors='replace').strip()
        if line:
            if not data_detected:
                print("✓ GPS data detected!")
                data_detected = True
            
            line_count += 1
            timestamp = time.strftime("%H:%M:%S")
            print(f"[{timestamp}] Line {line_count}: {line}")
        else:
            # No data received
            elapsed = time.time() - start_time
            if elapsed > 10 and not data_detected:
                print(f"⚠ No GPS data received after {elapsed:.1f}s - check connections")
                start_time = time.time()  # Reset timer
                
except KeyboardInterrupt:
    print(f"\n\n=== Session Summary ===")
    print(f"Total lines received: {line_count}")
    if data_detected:
        print("✓ GPS module detected and communicating")
    else:
        print("✗ No GPS data detected - check hardware connections")
finally:
    ser.close()
    if gps_en is not None:
        gps_en.off()  # Optionally power down GPS
        gps_en.close()
    print("Serial port closed and GPIO cleaned up.")
