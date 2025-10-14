"""
ST7789 Display Driver for Raspberry Pi Pico2
Compatible with Waveshare 1.3" LCD 240x240
"""

import time
from machine import Pin, SPI


class ST7789:
    def __init__(self, width=320, height=240, spi_id=1,
                 dc_pin=8, rst_pin=12, cs_pin=9, bl_pin=13):
        print("[ST7789] init start")
        self.width = width
        self.height = height
        self.rotation = 0

        # Select correct default pins for SPI0 / SPI1
        if spi_id == 0:
            sck, mosi = 2, 3
        else:
            sck, mosi = 10, 11

        # Initialize SPI
        self.spi = SPI(spi_id, baudrate=62500000, polarity=0, phase=0,
                       sck=Pin(sck), mosi=Pin(mosi), miso=None)

        # Control pins
        self.dc = Pin(dc_pin, Pin.OUT)
        self.rst = Pin(rst_pin, Pin.OUT)
        self.cs = Pin(cs_pin, Pin.OUT)
        self.bl = Pin(bl_pin, Pin.OUT)

        # Turn on backlight
        self.bl.value(1)

        # Initialize display
        self.init_display()
        
    def set_window(self, x0, y0, x1, y1):
        self.write_cmd(0x2A)
        self.write_data([x0 >> 8, x0 & 0xFF, (x1-1) >> 8, (x1-1) & 0xFF])
        self.write_cmd(0x2B)
        self.write_data([y0 >> 8, y0 & 0xFF, (y1-1) >> 8, (y1-1) & 0xFF])
        self.write_cmd(0x2C)

    def fill(self, color):
        self.set_window(0, 0, self.width, self.height)
        hi = color >> 8
        lo = color & 0xFF
        pixel = bytes([hi, lo])
        chunk = pixel * 128
        total_pixels = self.width * self.height
        blocks, remainder = divmod(total_pixels, 128)
        self.dc.value(1)
        self.cs.value(0)
        for _ in range(blocks):
            self.spi.write(chunk)
        if remainder:
            self.spi.write(pixel * remainder)
        self.cs.value(1)

    def show_image(self, image_data):
        if len(image_data) != 115200:
            print(f"Invalid image data size: {len(image_data)}")
            return
        self.set_window(0, 0, self.width, self.height)
        self.dc.value(1)
        self.cs.value(0)
        self.spi.write(image_data)
        self.cs.value(1)

    def blit_buffer(self, buffer, x, y, width, height):
        """Blit a buffer to the display at specified position"""
        # Set the window for the blit operation
        self.set_window(x, y, x + width, y + height)

        # Write the buffer data
        self.dc.value(1)
        self.cs.value(0)
        self.spi.write(buffer)
        self.cs.value(1)

    def draw_pixel(self, x, y, color):
        """Draw a single pixel at x, y with given color"""
        if x < 0 or x >= self.width or y < 0 or y >= self.height:
            return
        self.set_window(x, y, x+1, y+1)
        hi = color >> 8
        lo = color & 0xFF
        self.write_data([hi, lo])

    def draw_rect(self, x, y, w, h, color):
        """Draw a filled rectangle"""
        if x >= self.width or y >= self.height:
            return
        # Clip to screen bounds
        x1 = min(x + w, self.width)
        y1 = min(y + h, self.height)
        x = max(0, x)
        y = max(0, y)

        if x1 <= x or y1 <= y:
            return

        self.set_window(x, y, x1, y1)
        hi = color >> 8
        lo = color & 0xFF
        pixel_data = bytes([hi, lo])
        pixel_count = (x1 - x) * (y1 - y)

        # Send data in chunks to avoid large allocations
        chunk_size = 256
        full_chunks = pixel_count // chunk_size
        remainder = pixel_count % chunk_size

        chunk_data = pixel_data * chunk_size
        remainder_data = pixel_data * remainder

        self.dc.value(1)
        self.cs.value(0)
        for _ in range(full_chunks):
            self.spi.write(chunk_data)
        if remainder > 0:
            self.spi.write(remainder_data)
        self.cs.value(1)


    def write_cmd(self, cmd):
        self.dc.value(0)  # Command
        self.cs.value(0)
        self.spi.write(bytearray([cmd]))
        self.cs.value(1)

    def write_data(self, data):
        self.dc.value(1)  # Data
        self.cs.value(0)
        if isinstance(data, int):
            self.spi.write(bytearray([data]))
        elif isinstance(data, (list, tuple)):
            self.spi.write(bytearray(data))
        else:
            self.spi.write(data)  # already bytes/bytearray
        self.cs.value(1)

    def clear(self, color=0x0000):
        """Clear screen with given color (default black)"""
        self.fill(color)

    def color565(self, r, g, b):
        """Convert RGB888 to RGB565 color format"""
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

    def draw_line(self, x0, y0, x1, y1, color):
        """Draw a line using Bresenham's algorithm"""
        dx = abs(x1 - x0)
        dy = abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy

        while True:
            self.draw_pixel(x0, y0, color)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def reset(self):
        # Wait for power to stabilize after cold boot
        time.sleep_ms(200)
        self.rst.value(1)
        time.sleep_ms(100)
        self.rst.value(0)
        time.sleep_ms(100)
        self.rst.value(1)
        time.sleep_ms(200)

    def init_display(self):
        print("[ST7789] Starting reset sequence...")
        self.reset()
        print("[ST7789] Reset complete")

        try:
            # Software reset first (important for cold boot)
            print("[ST7789] Sending software reset...")
            self.write_cmd(0x01)
            time.sleep_ms(150)
            print("[ST7789] Software reset complete")

            # Sleep out
            print("[ST7789] Sending sleep out command...")
            self.write_cmd(0x11)
            time.sleep_ms(120)
            print("[ST7789] Sleep out complete")

            # Memory access control - fix rotation and color order
            self.write_cmd(0x36)
            self.write_data(0xA0)  # Row/Col exchange + RGB order + 180° rotation

            # Pixel format (16bit / RGB565)
            self.write_cmd(0x3A)
            self.write_data(0x05)

            # Porch setting
            self.write_cmd(0xB2)
            self.write_data([0x0C, 0x0C, 0x00, 0x33, 0x33])

            # Gate control
            self.write_cmd(0xB7)
            self.write_data(0x35)

            # VCOM setting
            self.write_cmd(0xBB)
            self.write_data(0x19)

            # LCM control
            self.write_cmd(0xC0)
            self.write_data(0x2C)

            # VDV and VRH command enable
            self.write_cmd(0xC2)
            self.write_data(0x01)

            # VRH set
            self.write_cmd(0xC3)
            self.write_data(0x12)

            # VDV set
            self.write_cmd(0xC4)
            self.write_data(0x20)

            # Frame rate control
            self.write_cmd(0xC6)
            self.write_data(0x0F)

            # Power control
            self.write_cmd(0xD0)
            self.write_data([0xA4, 0xA1])

            # Positive gamma correction
            self.write_cmd(0xE0)
            gamma_p = [0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B,
                       0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B,
                       0x1F, 0x23]
            for val in gamma_p:
                self.write_data(val)

            # Negative gamma correction
            self.write_cmd(0xE1)
            gamma_n = [0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C,
                       0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F,
                       0x20, 0x23]
            for val in gamma_n:
                self.write_data(val)

            # Display inversion on (to make white background black)
            self.write_cmd(0x21)

            # Display on
            print("[ST7789] Turning display on...")
            self.write_cmd(0x29)
            time.sleep_ms(100)
            print("[ST7789] Display initialization complete!")
        except Exception as e:
            print(f"[ST7789] Init error: {e}")
            import sys
            sys.print_exception(e)

    # --- the rest of your methods unchanged ---

