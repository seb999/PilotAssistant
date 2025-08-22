import cv2
from PIL import Image
import time
from picamera2 import Picamera2
from library import ST7789

def display_stream_page(lcd):
    print("Displaying stream page...")

    # lcd = ST7789.ST7789()
    # lcd.Init()
    # lcd.clear()
    # lcd.bl_DutyCycle(50)

    # # Initialize camera
    picam2 = Picamera2()
    picam2.start()
    time.sleep(1)

    try:
        while True:
            frame = picam2.capture_array()
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame).resize((240, 240)).convert("RGB").rotate(270)
            # cv2.imshow("Original Camera Feed", frame)
            # if cv2.waitKey(1) == ord('q'):
            #     break

            lcd.ShowImage(img)

            if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
                print("Center button pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break

            # if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
            #     if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
            #         while lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
            #             time.sleep(0.1)
            #         break

            # time.sleep(0.01)  # Small delay to prevent CPU overload
    finally:
        lcd.clear()
