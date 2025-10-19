import serial
import json
import time
import random

# === CONFIGURATION ===
SERIAL_PORT = "/dev/cu.usbmodem14101"
BAUDRATE = 115200
UPDATE_RATE_HZ = 1
COPENHAGEN_LAT, COPENHAGEN_LON = 55.6180, 12.6560

# === INITIALIZE SERIAL ===
print(f"Connecting to {SERIAL_PORT}...")
ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
print(f"Connected to {SERIAL_PORT} at {BAUDRATE} baud.")

# === MAIN LOOP ===
interval = 1.0 / UPDATE_RATE_HZ

try:
    count = 0
    while True:
        count += 1

        # Simulate 3-8 aircraft around Copenhagen
        num_aircraft = random.randint(3, 8)
        traffic_json = []

        for i in range(num_aircraft):
            traffic_json.append({
                "id": f"SAS{100+i}",
                "lat": COPENHAGEN_LAT + random.uniform(-0.2, 0.2),  # ~±20km
                "lon": COPENHAGEN_LON + random.uniform(-0.2, 0.2),  # ~±20km
                "alt": random.randint(1000, 10000)  # 1000-10000m altitude
            })

        # Complete telemetry JSON structure
        telemetry = {
            "own": {
                "lat": COPENHAGEN_LAT,
                "lon": COPENHAGEN_LON,
                "alt": 0,
                "pitch": 0,
                "roll": 0,
                "yaw": 0
            },
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
        print(f"[{count}] Sent {len(traffic_json)} simulated aircraft")

        time.sleep(interval)

except KeyboardInterrupt:
    print("\nStopping telemetry stream.")
finally:
    ser.close()
