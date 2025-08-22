import serial
import RPi.GPIO as GPIO
import time
import sys
import os

# Add the library directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from config import GPS_EN_PIN, GPS_PORT, GPS_BAUDRATE, GPS_TIMEOUT

# --- SETUP ---

if GPS_EN_PIN is not None:
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(GPS_EN_PIN, GPIO.OUT)
    GPIO.output(GPS_EN_PIN, GPIO.HIGH)  # Power on the GPS
    print(f"GPS EN pin set to HIGH (GPIO{GPS_EN_PIN})")
    time.sleep(1)  # Give GPS time to boot

# Open the GPS serial port
try:
    ser = serial.Serial(GPS_PORT, GPS_BAUDRATE, timeout=GPS_TIMEOUT)
    print(f"Listening for GPS data on {GPS_PORT} at {GPS_BAUDRATE} baud...")
except serial.SerialException as e:
    print(f"Error opening serial port {GPS_PORT}: {e}")
    exit(1)

# --- MAIN LOOP ---

try:
    while True:
        line = ser.readline().decode('ascii', errors='replace').strip()
        if line:
            print(line)
except KeyboardInterrupt:
    print("\nStopped by user.")
finally:
    ser.close()
    if GPS_EN_PIN is not None:
        GPIO.output(GPS_EN_PIN, GPIO.LOW)  # Optionally power down GPS
        GPIO.cleanup()
    print("Serial closed. GPIO cleaned up.")
