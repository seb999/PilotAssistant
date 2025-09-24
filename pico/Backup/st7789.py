"""
ST7789 Display Driver for Raspberry Pi Pico2
Compatible with Waveshare 1.3" LCD 240x240
"""

import time
from machine import Pin, SPI


class ST7789:
    def __init__(self, width=240, height=240, spi_id=1,
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
            debug(f"Invalid image data size: {len(image_data)}")
            return
        self.set_window(0, 0, self.width, self.height)
        self.dc.value(1)
        self.cs.value(0)
        self.spi.write(image_data)
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

    def reset(self):
        self.rst.value(1)
        time.sleep_ms(50)
        self.rst.value(0)
        time.sleep_ms(50)
        self.rst.value(1)
        time.sleep_ms(50)

    def init_display(self):
        self.reset()

        try:
            # Sleep out
            self.write_cmd(0x11)
            time.sleep_ms(120)

            # Memory access control
            self.write_cmd(0x36)
            self.write_data(0x00)

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

            # Display on
            self.write_cmd(0x29)
            time.sleep_ms(100)
        except Exception as e:
            print(e)

    # --- the rest of your methods unchanged ---

