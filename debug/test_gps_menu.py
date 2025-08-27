#!/usr/bin/env python3
import sys
import os
import logging

# Add the parent directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from library import ST7789
from menu.gps_menu import display_gps_page

logging.basicConfig(level=logging.DEBUG)

print("Testing GPS Menu...")

# Initialize LCD
lcd = ST7789.ST7789()
lcd.Init()
lcd.clear()
lcd.bl_DutyCycle(50)

try:
    # Launch GPS menu page
    display_gps_page(lcd)
    
finally:
    # Clean up
    lcd.clear()
    lcd.module_exit()
    print("GPS menu test completed")