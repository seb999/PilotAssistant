from library import ST7789
from PIL import Image
import time
from picamera2 import Picamera2
import cv2 

# Initialize the display
lcd = ST7789.ST7789()
lcd.Init()
lcd.clear()
lcd.bl_DutyCycle(50)

# Initialize camera
picam2 = Picamera2()
picam2.start()
time.sleep(1)

# Display loop
try:
    while True:
        frame = picam2.capture_array()
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        img = Image.fromarray(frame).resize((240, 240)).convert("RGB").rotate(270)
        # cv2.imshow("Original Camera Feed", frame)
        # if cv2.waitKey(1) == ord('q'):
        #     break

        lcd.ShowImage(img)
except KeyboardInterrupt:
    print("Stopped.")
