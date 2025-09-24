"""
Input Handler for Raspberry Pi Pico2
Handles joystick and button inputs with edge detection and debounce.
Safe GPIO pins, compatible with Pico2 and RPi PilotAssistant.
"""

from machine import Pin
import time

class InputHandler:
    DEBOUNCE_MS = 50  # debounce time in milliseconds

    def __init__(self, pin_map=None):
        """
        Initialize joystick and button pins.
        Optionally, pass a dictionary `pin_map` to override default GPIO assignments.
        """
        # Default safe pins (avoiding SPI/LCD pins: 8–13)
        default_pins = {
            'up': 2,
            'press': 3,
            'down': 18,
            'left': 16,
            'right': 20,
            'key1': 15,
            'key2': 17,
            'key3': 19,
            'key4': 21
        }

        # Use custom pin map if provided
        if pin_map:
            default_pins.update(pin_map)

        # Initialize pins
        self.pins = {name: Pin(gpio, Pin.IN, Pin.PULL_UP) for name, gpio in default_pins.items()}

        # Store last states for edge detection
        self.last_states = {name: 1 for name in self.pins}

        # Last change timestamp for debounce
        self.last_times = {name: 0 for name in self.pins}

    def read_inputs(self):
        """
        Read all input states and return a list of changes.
        Each item is a tuple: (button_name, new_state)
        0 = pressed, 1 = released
        """
        changes = []
        current_time = time.ticks_ms()

        for name, pin in self.pins.items():
            state = pin.value()
            last_state = self.last_states[name]
            last_time = self.last_times[name]

            # Only register if state changed and debounce passed
            if state != last_state and time.ticks_diff(current_time, last_time) > self.DEBOUNCE_MS:
                self.last_states[name] = state
                self.last_times[name] = current_time
                changes.append((name, state))

        return changes

