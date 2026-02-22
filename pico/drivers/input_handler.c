#include "input_handler.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>

void input_init(void) {
    // Initialize digital button pins with pull-up resistors
    gpio_init(PIN_KEY1);
    gpio_set_dir(PIN_KEY1, GPIO_IN);
    gpio_pull_up(PIN_KEY1);

    gpio_init(PIN_KEY2);
    gpio_set_dir(PIN_KEY2, GPIO_IN);
    gpio_pull_up(PIN_KEY2);

    gpio_init(PIN_KEY4);
    gpio_set_dir(PIN_KEY4, GPIO_IN);
    gpio_pull_up(PIN_KEY4);

    gpio_init(PIN_JOY_SW);
    gpio_set_dir(PIN_JOY_SW, GPIO_IN);
    gpio_pull_up(PIN_JOY_SW);

    // Initialize ADC for joystick
    adc_init();

    // Configure ADC0 (GPIO 26) for Y-axis
    adc_gpio_init(PIN_JOY_VRY);

    // Configure ADC1 (GPIO 27) for X-axis
    adc_gpio_init(PIN_JOY_VRX);

    printf("Input handler initialized\n");
    printf("  Digital buttons: KEY1=%d, KEY2=%d, KEY4=%d, JOY_SW=%d\n",
           PIN_KEY1, PIN_KEY2, PIN_KEY4, PIN_JOY_SW);
    printf("  Analog joystick: VRX=%d (ADC1), VRY=%d (ADC0)\n",
           PIN_JOY_VRX, PIN_JOY_VRY);
}

static bool is_debounced(uint32_t last_time, uint32_t current_time) {
    return (current_time - last_time) > DEBOUNCE_MS;
}

void input_read(InputState* state) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    // Save previous states
    state->up_prev = state->up;
    state->down_prev = state->down;
    state->left_prev = state->left;
    state->right_prev = state->right;
    state->press_prev = state->press;
    state->key1_prev = state->key1;
    state->key2_prev = state->key2;
    state->key4_prev = state->key4;

    // Read digital buttons (active low - 0 = pressed)
    bool key1_raw = !gpio_get(PIN_KEY1);
    bool key2_raw = !gpio_get(PIN_KEY2);
    bool key4_raw = !gpio_get(PIN_KEY4);
    bool press_raw = !gpio_get(PIN_JOY_SW);

    // Update digital buttons with debouncing
    if (key1_raw != state->key1 && is_debounced(state->key1_time, current_time)) {
        state->key1 = key1_raw;
        state->key1_time = current_time;
    }

    if (key2_raw != state->key2 && is_debounced(state->key2_time, current_time)) {
        state->key2 = key2_raw;
        state->key2_time = current_time;
    }

    if (key4_raw != state->key4 && is_debounced(state->key4_time, current_time)) {
        state->key4 = key4_raw;
        state->key4_time = current_time;
    }

    if (press_raw != state->press && is_debounced(state->press_time, current_time)) {
        state->press = press_raw;
        state->press_time = current_time;
    }

    // Read joystick analog values
    // Read X-axis (ADC1 = GPIO 27)
    adc_select_input(1);
    uint16_t x_raw = adc_read();

    // Read Y-axis (ADC0 = GPIO 26)
    adc_select_input(0);
    uint16_t y_raw = adc_read();

    // Debug output every 1000 reads (to avoid spam)
    static int debug_counter = 0;
    if (debug_counter++ % 1000 == 0) {
        printf("ADC: X=%u Y=%u (center range: %u-%u)\n",
               x_raw, y_raw, JOYSTICK_CENTER_MIN, JOYSTICK_CENTER_MAX);
    }

    // Determine joystick directions
    bool left_raw = x_raw < JOYSTICK_CENTER_MIN;
    bool right_raw = x_raw > JOYSTICK_CENTER_MAX;
    bool up_raw = y_raw < JOYSTICK_CENTER_MIN;
    bool down_raw = y_raw > JOYSTICK_CENTER_MAX;

    // Update joystick directions with debouncing
    if (left_raw != state->left && is_debounced(state->left_time, current_time)) {
        state->left = left_raw;
        state->left_time = current_time;
    }

    if (right_raw != state->right && is_debounced(state->right_time, current_time)) {
        state->right = right_raw;
        state->right_time = current_time;
    }

    if (up_raw != state->up && is_debounced(state->up_time, current_time)) {
        state->up = up_raw;
        state->up_time = current_time;
    }

    if (down_raw != state->down && is_debounced(state->down_time, current_time)) {
        state->down = down_raw;
        state->down_time = current_time;
    }
}

// Edge detection helpers - detect button press (transition from false to true)
bool input_just_pressed_up(InputState* state) {
    return state->up && !state->up_prev;
}

bool input_just_pressed_down(InputState* state) {
    return state->down && !state->down_prev;
}

bool input_just_pressed_left(InputState* state) {
    return state->left && !state->left_prev;
}

bool input_just_pressed_right(InputState* state) {
    return state->right && !state->right_prev;
}

bool input_just_pressed_press(InputState* state) {
    return state->press && !state->press_prev;
}

bool input_just_pressed_key1(InputState* state) {
    return state->key1 && !state->key1_prev;
}

bool input_just_pressed_key2(InputState* state) {
    return state->key2 && !state->key2_prev;
}

bool input_just_pressed_key4(InputState* state) {
    return state->key4 && !state->key4_prev;
}
