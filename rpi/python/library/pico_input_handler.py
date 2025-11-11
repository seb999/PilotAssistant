"""
Enhanced Input Handler with Pico2 Support
Provides unified input handling from both local RPi buttons and forwarded Pico2 buttons
"""

import time
from library.config import RaspberryPi

class PicoInputHandler(RaspberryPi):
    def __init__(self, pico_mirror=None):
        super().__init__()
        self.pico_mirror = pico_mirror
        self.pico_input_enabled = False
        
        # Virtual button states for Pico2 inputs
        self.pico_button_states = {
            'KEY_UP_PIN': 1,
            'KEY_DOWN_PIN': 1,
            'KEY_LEFT_PIN': 1,
            'KEY_RIGHT_PIN': 1,
            'KEY_PRESS_PIN': 1,
            'KEY1_PIN': 1,
            'KEY2_PIN': 1,
            'KEY3_PIN': 1
        }
        
        # Register input handlers with pico mirror if available
        if self.pico_mirror:
            self._register_pico_handlers()
            
    def _register_pico_handlers(self):
        """Register button event handlers with Pico2 mirror"""
        for button in self.pico_button_states.keys():
            self.pico_mirror.register_input_handler(button, self._handle_pico_input)
            
    def _handle_pico_input(self, button, state, raw_state):
        """Handle input from Pico2"""
        if button in self.pico_button_states:
            self.pico_button_states[button] = raw_state
            
    def enable_pico_input(self):
        """Enable Pico2 input forwarding"""
        self.pico_input_enabled = True
        print("Pico2 input forwarding enabled")
        
    def disable_pico_input(self):
        """Disable Pico2 input forwarding"""
        self.pico_input_enabled = False
        print("Pico2 input forwarding disabled")
        
    def digital_read_unified(self, button_name):
        """Read button state from either local RPi or Pico2"""
        # If Pico2 input is enabled and available, use it
        if self.pico_input_enabled and self.pico_mirror and self.pico_mirror.is_connected():
            if button_name == 'UP':
                return self.pico_button_states['KEY_UP_PIN']
            elif button_name == 'DOWN':
                return self.pico_button_states['KEY_DOWN_PIN']
            elif button_name == 'LEFT':
                return self.pico_button_states['KEY_LEFT_PIN']
            elif button_name == 'RIGHT':
                return self.pico_button_states['KEY_RIGHT_PIN']
            elif button_name == 'PRESS':
                return self.pico_button_states['KEY_PRESS_PIN']
            elif button_name == 'KEY1':
                return self.pico_button_states['KEY1_PIN']
            elif button_name == 'KEY2':
                return self.pico_button_states['KEY2_PIN']
            elif button_name == 'KEY3':
                return self.pico_button_states['KEY3_PIN']
                
        # Fall back to local RPi buttons
        if button_name == 'UP':
            return self.digital_read(self.GPIO_KEY_UP_PIN)
        elif button_name == 'DOWN':
            return self.digital_read(self.GPIO_KEY_DOWN_PIN)
        elif button_name == 'LEFT':
            return self.digital_read(self.GPIO_KEY_LEFT_PIN)
        elif button_name == 'RIGHT':
            return self.digital_read(self.GPIO_KEY_RIGHT_PIN)
        elif button_name == 'PRESS':
            return self.digital_read(self.GPIO_KEY_PRESS_PIN)
        elif button_name == 'KEY1':
            return self.digital_read(self.GPIO_KEY1_PIN)
        elif button_name == 'KEY2':
            return self.digital_read(self.GPIO_KEY2_PIN)
        elif button_name == 'KEY3':
            return self.digital_read(self.GPIO_KEY3_PIN)
            
        return 1  # Default to "not pressed"