import sys
import os
# Add parent directory to path to find library module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from library import ST7789
from PIL import Image
import time
from picamera2 import Picamera2

# Initialize the display
lcd = ST7789.ST7789()
lcd.Init()
lcd.clear()
lcd.bl_DutyCycle(50)

# Initialize camera
picam2 = Picamera2()
config = picam2.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
picam2.configure(config)
picam2.start()
time.sleep(1)

# Display loop
try:
    while True:
        frame = picam2.capture_array()
        img = Image.fromarray(frame).resize((240, 240)).convert("RGB").rotate(270)
        lcd.ShowImage(img)
except KeyboardInterrupt:
    print("Stopped.")
    picam2.stop()
    lcd.clear()
