import cv2
from PIL import Image
import time
from picamera2 import Picamera2
from library import ST7789

def display_stream_page(lcd):
    print("Displaying stream page...")

    # Import Pico2 button states from library module
    try:
        import library.pico_state
        pico2_button_states = library.pico_state.pico2_button_states
    except:
        # Fallback if library module not available
        pico2_button_states = {
            'up_pressed': False,
            'down_pressed': False,
            'press_pressed': False,
            'key1_pressed': False,
            'key2_pressed': False
        }

    # # Initialize camera
    picam2 = Picamera2()
    # Configure camera to output RGB format
    config = picam2.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
    picam2.configure(config)
    picam2.start()
    time.sleep(1)
    
    # Initialize KEY1 state tracking
    last_key1 = lcd.digital_read(lcd.GPIO_KEY1_PIN)
    
    # Initialize joystick state tracking for exit functionality
    last_up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
    last_down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
    last_left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
    last_right = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)

    # Pico2 button state tracking for edge detection
    # Initialize with current state to avoid immediate trigger from menu selection
    last_pico2_states = pico2_button_states.copy()
    # Ensure all expected keys exist in last_pico2_states
    for key in ['up_pressed', 'down_pressed', 'left_pressed', 'right_pressed', 'press_pressed', 'key1_pressed', 'key2_pressed']:
        if key not in last_pico2_states:
            last_pico2_states[key] = False

    # Small delay to avoid immediate exit from menu selection button press
    time.sleep(0.2)

    try:
        while True:
            frame = picam2.capture_array()
            # Swap red and blue channels to fix color issue
            frame[:, :, [0, 2]] = frame[:, :, [2, 0]]
            img = Image.fromarray(frame).resize((240, 240)).convert("RGB").rotate(270)
            # cv2.imshow("Original Camera Feed", frame)
            # if cv2.waitKey(1) == ord('q'):
            #     break

            lcd.ShowImage(img)

            # Check center button press (LCD or Pico2)
            pico2_press_pressed = pico2_button_states['press_pressed'] and not last_pico2_states['press_pressed']
            if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1 or pico2_press_pressed:
                print("Center button pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break

            # Handle joystick presses to exit to main menu (LCD or Pico2)
            up = lcd.digital_read(lcd.GPIO_KEY_UP_PIN)
            pico2_up_pressed = pico2_button_states['up_pressed'] and not last_pico2_states['up_pressed']
            if (up == 0 and last_up == 1) or pico2_up_pressed:
                print("UP pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break
            last_up = up

            down = lcd.digital_read(lcd.GPIO_KEY_DOWN_PIN)
            pico2_down_pressed = pico2_button_states['down_pressed'] and not last_pico2_states['down_pressed']
            if (down == 0 and last_down == 1) or pico2_down_pressed:
                print("DOWN pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break
            last_down = down

            left = lcd.digital_read(lcd.GPIO_KEY_LEFT_PIN)
            if left == 0 and last_left == 1:
                print("LEFT pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break
            last_left = left

            right = lcd.digital_read(lcd.GPIO_KEY_RIGHT_PIN)
            if right == 0 and last_right == 1:
                print("RIGHT pressed, exiting stream page.")
                picam2.stop()
                picam2.close()
                break
            last_right = right
                
            # Handle KEY1 press to switch to gyro page (LCD or Pico2)
            key1 = lcd.digital_read(lcd.GPIO_KEY1_PIN)
            pico2_key1_pressed = pico2_button_states['key1_pressed'] and not last_pico2_states['key1_pressed']
            if (key1 == 0 and last_key1 == 1) or pico2_key1_pressed:
                print("KEY1 pressed → switching to gyro page")
                picam2.stop()
                picam2.close()
                # Dynamic import to avoid circular dependency
                from menu.gyro_menu import display_setup_page
                from PIL import ImageFont
                font4 = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 18)
                display_setup_page(lcd, font4)
                # Re-initialize camera after returning from gyro page
                picam2 = Picamera2()
                picam2.start()
                time.sleep(1)
            last_key1 = key1

            # Update Pico2 button states for edge detection
            last_pico2_states = pico2_button_states.copy()

            # if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 0:
            #     if lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
            #         while lcd.digital_read(lcd.GPIO_KEY_PRESS_PIN) == 1:
            #             time.sleep(0.1)
            #         break

            # time.sleep(0.01)  # Small delay to prevent CPU overload
    finally:
        lcd.clear()
