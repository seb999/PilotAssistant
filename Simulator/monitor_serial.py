#!/usr/bin/env python3
"""
Simple serial monitor to see all output from Pico
"""
import serial
import sys

SERIAL_PORT = "/dev/cu.usbmodem14101"
BAUDRATE = 115200

print(f"Monitoring {SERIAL_PORT} at {BAUDRATE} baud")
print("Press Ctrl+C to exit\n")
print("=" * 60)

try:
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)

    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            print(line)

except KeyboardInterrupt:
    print("\n\nStopping monitor.")
except Exception as e:
    print(f"Error: {e}")
finally:
    try:
        ser.close()
    except:
        pass
