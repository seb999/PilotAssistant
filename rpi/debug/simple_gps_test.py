#!/usr/bin/env python3
import serial
import time

print("Simple GPS Test")
print("Attempting to read from /dev/ttyAMA0...")

try:
    # Open serial port
    ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=2)
    print("Serial port opened successfully")
    
    # Try to read data for 5 seconds
    start_time = time.time()
    lines_received = 0
    
    while time.time() - start_time < 5:
        try:
            line = ser.readline().decode('ascii', errors='replace').strip()
            if line:
                lines_received += 1
                print(f"Line {lines_received}: {line}")
        except Exception as e:
            print(f"Read error: {e}")
            break
    
    if lines_received == 0:
        print("No GPS data received - GPS may be off or not connected")
    else:
        print(f"Received {lines_received} lines of GPS data")
        
except serial.SerialException as e:
    print(f"Serial port error: {e}")
except Exception as e:
    print(f"Error: {e}")
finally:
    try:
        ser.close()
        print("Serial port closed")
    except:
        pass