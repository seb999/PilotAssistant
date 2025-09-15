# Shared Pico2 button state module
# This module provides access to Pico2 button states across all menu pages

# Global Pico2 button state variables
pico2_button_states = {
    'up_pressed': False,
    'down_pressed': False,
    'left_pressed': False,
    'right_pressed': False,
    'press_pressed': False,
    'key1_pressed': False,
    'key2_pressed': False
}

# State tracking for edge detection
last_pico2_states = {
    'up_pressed': False,
    'down_pressed': False,
    'left_pressed': False,
    'right_pressed': False,
    'press_pressed': False,
    'key1_pressed': False,
    'key2_pressed': False
}

def check_pico2_button_press(button_name):
    """Check if a Pico2 button was just pressed (edge detection)"""
    current = pico2_button_states.get(button_name, False)
    last = last_pico2_states.get(button_name, False)
    return current and not last

def update_pico2_states():
    """Update last states for edge detection - call after checking all buttons"""
    global last_pico2_states
    last_pico2_states = pico2_button_states.copy()