"""
Screen mirroring module to automatically sync RPi display with Pico screen
"""

import serial
import threading
import time
from PIL import Image

class ScreenMirror:
    def __init__(self, pico_port="/dev/ttyACM0", pico_baudrate=115200):
        self.pico_port = pico_port
        self.pico_baudrate = pico_baudrate
        self.mirror_enabled = True
        self.mirror_quality = 1.0  # 1.0 = full quality, 0.5 = half quality, etc.
        self.send_queue = []
        self.send_lock = threading.Lock()
        self._start_sender_thread()

    def _start_sender_thread(self):
        """Start background thread for sending images to Pico"""
        def sender_worker():
            while True:
                try:
                    with self.send_lock:
                        if self.send_queue:
                            image = self.send_queue.pop(0)  # Get oldest image
                        else:
                            image = None

                    if image and self.mirror_enabled:
                        self._send_image_to_pico(image)

                    time.sleep(0.016)  # ~60fps max
                except Exception as e:
                    print(f"Screen mirror sender error: {e}")
                    time.sleep(0.1)

        sender_thread = threading.Thread(target=sender_worker, daemon=True)
        sender_thread.start()

    def mirror_image(self, image):
        """Queue image for mirroring to Pico (non-blocking)"""
        if not self.mirror_enabled:
            return

        try:
            # Apply quality scaling if needed
            if self.mirror_quality < 1.0:
                new_size = (int(240 * self.mirror_quality), int(240 * self.mirror_quality))
                image_scaled = image.resize(new_size).resize((240, 240))
            else:
                image_scaled = image

            with self.send_lock:
                # Keep queue small to avoid lag
                if len(self.send_queue) > 2:
                    self.send_queue = self.send_queue[-1:]  # Keep only latest
                self.send_queue.append(image_scaled.copy())

        except Exception as e:
            print(f"Error queuing image for mirroring: {e}")

    def _send_image_to_pico(self, image):
        """Send PIL image to Pico2 for display"""
        try:
            # Resize image to 240x240 to match Pico2 display
            image_resized = image.resize((240, 240))

            # Convert to RGB if not already
            if image_resized.mode != 'RGB':
                image_resized = image_resized.convert('RGB')

            # Convert to 16-bit RGB565 format that ST7789 expects
            pixels = []
            for y in range(240):
                for x in range(240):
                    r, g, b = image_resized.getpixel((x, y))
                    # Convert RGB888 to RGB565
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    # Pack as 2 bytes (big-endian)
                    pixels.append((rgb565 >> 8) & 0xFF)  # High byte
                    pixels.append(rgb565 & 0xFF)         # Low byte

            # Send to Pico2 using the existing frame protocol
            frame_data = bytes(pixels)
            frame_message = b"FRAME:240x240:" + frame_data + b"\n"

            # Send with short timeout to avoid blocking
            ser = serial.Serial(self.pico_port, self.pico_baudrate, timeout=0.1)
            ser.write(frame_message)
            ser.close()

        except Exception as e:
            print(f"Error sending image to Pico: {e}")

    def enable_mirroring(self):
        """Enable screen mirroring"""
        self.mirror_enabled = True
        print("Screen mirroring enabled")

    def disable_mirroring(self):
        """Disable screen mirroring"""
        self.mirror_enabled = False
        print("Screen mirroring disabled")

    def set_quality(self, quality):
        """Set mirroring quality (0.1 to 1.0)"""
        self.mirror_quality = max(0.1, min(1.0, quality))
        print(f"Screen mirroring quality set to {self.mirror_quality}")

# Global screen mirror instance
screen_mirror = ScreenMirror()