# boot.py - Ensures main.py runs on startup
import machine
import time

print("=" * 40)
print("BOOT.PY STARTING...")
print("=" * 40)

# Quick LED blink to show boot.py is running
try:
    led = machine.Pin(25, machine.Pin.OUT)
    for _ in range(2):
        led.on()
        time.sleep(0.1)
        led.off()
        time.sleep(0.1)
    print("Boot LED blink complete")
except Exception as e:
    print(f"LED error: {e}")

print("boot.py complete - main.py should start now")
print("=" * 40)
