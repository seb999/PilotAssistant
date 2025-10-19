"""
Input Handler for Raspberry Pi Pico2
Handles joystick and button inputs with edge detection and debounce.
Safe GPIO pins, compatible with Pico2 and RPi PilotAssistant.
"""

from machine import Pin, ADC
import time

class InputHandler:
    DEBOUNCE_MS = 50  # debounce time in milliseconds

    def __init__(self, pin_map, joystick_config=None):
        """
        Initialize joystick and button pins.
        pin_map: dictionary with GPIO assignments for buttons
        joystick_config: dict with 'vrx', 'vry', 'sw' pin numbers and thresholds
        """
        # Initialize digital pins from provided pin map
        self.pins = {name: Pin(gpio, Pin.IN, Pin.PULL_UP) for name, gpio in pin_map.items()}

        # Store last states for edge detection
        self.last_states = {name: 1 for name in self.pins}

        # Last change timestamp for debounce
        self.last_times = {name: 0 for name in self.pins}

        # Initialize ADC joystick if provided
        self.joystick = None
        if joystick_config:
            self.joystick = {
                'vrx': ADC(Pin(joystick_config['vrx'])),
                'vry': ADC(Pin(joystick_config['vry'])),
                'sw': Pin(joystick_config['sw'], Pin.IN, Pin.PULL_UP),
                'center_min': joystick_config.get('center_min', 25000),
                'center_max': joystick_config.get('center_max', 40000)
            }
            # Add joystick directions to tracking
            for direction in ['up', 'down', 'left', 'right', 'press']:
                if direction not in self.last_states:
                    self.last_states[direction] = 1
                    self.last_times[direction] = 0

    def read_inputs(self):
        """
        Read all input states and return a list of changes.
        Each item is a tuple: (button_name, new_state)
        0 = pressed, 1 = released
        """
        changes = []
        current_time = time.ticks_ms()

        # Read digital buttons
        for name, pin in self.pins.items():
            state = pin.value()
            last_state = self.last_states[name]
            last_time = self.last_times[name]

            # Only register if state changed and debounce passed
            if state != last_state and time.ticks_diff(current_time, last_time) > self.DEBOUNCE_MS:
                self.last_states[name] = state
                self.last_times[name] = current_time
                changes.append((name, state))

        # Read ADC joystick if available
        if self.joystick:
            x = self.joystick['vrx'].read_u16()
            y = self.joystick['vry'].read_u16()
            button = not self.joystick['sw'].value()

            center_min = self.joystick['center_min']
            center_max = self.joystick['center_max']

            # Check each direction
            directions = {
                'left': x < center_min,
                'right': x > center_max,
                'up': y < center_min,
                'down': y > center_max,
                'press': button
            }

            for direction, is_active in directions.items():
                state = 0 if is_active else 1  # 0 = pressed, 1 = released
                last_state = self.last_states[direction]
                last_time = self.last_times[direction]

                if state != last_state and time.ticks_diff(current_time, last_time) > self.DEBOUNCE_MS:
                    self.last_states[direction] = state
                    self.last_times[direction] = current_time
                    changes.append((direction, state))

        return changes

