#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Debounce time in milliseconds
#define DEBOUNCE_MS 50

// Joystick ADC thresholds (12-bit ADC on RP2350: 0-4095)
// Center is around 2048 (middle of 12-bit range)
#define JOYSTICK_CENTER_MIN 1500  // Below this = LEFT or UP
#define JOYSTICK_CENTER_MAX 2600  // Above this = RIGHT or DOWN

// Pin definitions
#define PIN_KEY1 2
#define PIN_KEY2 3
#define PIN_KEY4 15
#define PIN_JOY_SW 16
#define PIN_JOY_VRX 27  // ADC1 (GPIO 27)
#define PIN_JOY_VRY 26  // ADC0 (GPIO 26)

// ADC channel definitions for RP2350
#define ADC_CHANNEL_VRX 1  // GPIO 27 = ADC1
#define ADC_CHANNEL_VRY 0  // GPIO 26 = ADC0

// Input state structure
typedef struct {
    // Current button states (true = pressed)
    bool up;
    bool down;
    bool left;
    bool right;
    bool press;
    bool key1;
    bool key2;
    bool key4;

    // Previous states for edge detection
    bool up_prev;
    bool down_prev;
    bool left_prev;
    bool right_prev;
    bool press_prev;
    bool key1_prev;
    bool key2_prev;
    bool key4_prev;

    // Timestamps for debouncing (in milliseconds)
    uint32_t up_time;
    uint32_t down_time;
    uint32_t left_time;
    uint32_t right_time;
    uint32_t press_time;
    uint32_t key1_time;
    uint32_t key2_time;
    uint32_t key4_time;
} InputState;

// Initialize input handler (GPIO and ADC setup)
void input_init(void);

// Read all inputs and update state with edge detection and debouncing
void input_read(InputState* state);

// Check if a button was just pressed (rising edge)
bool input_just_pressed_up(InputState* state);
bool input_just_pressed_down(InputState* state);
bool input_just_pressed_left(InputState* state);
bool input_just_pressed_right(InputState* state);
bool input_just_pressed_press(InputState* state);
bool input_just_pressed_key1(InputState* state);
bool input_just_pressed_key2(InputState* state);
bool input_just_pressed_key4(InputState* state);

#endif // INPUT_HANDLER_H
