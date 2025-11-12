/**
 * Pico2 Command Sender
 * Sends button and joystick commands to Raspberry Pi over USB CDC serial
 *
 * Command Format:
 *   BTN:1,PRESS    - Button 1 pressed
 *   BTN:1,RELEASE  - Button 1 released
 *   JOY:UP         - Joystick moved up
 *   JOY:DOWN       - Joystick moved down
 *   JOY:LEFT       - Joystick moved left
 *   JOY:RIGHT      - Joystick moved right
 *   CMD:FLY_MODE   - High-level command (triggered by specific button combinations)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "input_handler.h"
#include "st7789_lcd.h"
#include "splash_data.h"

#define LED_PIN 25

// Track button release events
typedef struct {
    bool key1_was_pressed;
    bool key2_was_pressed;
    bool key4_was_pressed;
    bool press_was_pressed;
} ButtonReleaseTracker;

// Send button command over USB serial
void send_button_command(uint8_t button_id, const char* action) {
    printf("BTN:%d,%s\n", button_id, action);
    fflush(stdout);
}

// Send joystick command over USB serial
void send_joystick_command(const char* direction) {
    printf("JOY:%s\n", direction);
    fflush(stdout);
}

// Send high-level command over USB serial
void send_high_level_command(const char* command) {
    printf("CMD:%s\n", command);
    fflush(stdout);
}

int main() {
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Initialize USB serial (stdio)
    stdio_init_all();

    // Wait for USB connection
    sleep_ms(2000);

    printf("\n\n");
    printf("=====================================\n");
    printf("  Pico2 Command Sender v1.0\n");
    printf("  Sending Commands to Raspberry Pi\n");
    printf("=====================================\n");

    // Initialize LCD
    printf("Initializing LCD...\n");
    lcd_init();

    // Display splash screen for 2 seconds
    printf("Displaying splash screen...\n");
    lcd_display_splash(splash_320x240_bin, splash_320x240_bin_len);
    sleep_ms(2000);

    // Initialize input handler
    printf("Initializing input handler...\n");
    input_init();

    // Clear screen and show instructions
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(10, 10, "COMMAND SENDER", COLOR_CYAN, COLOR_BLACK);
    lcd_draw_string(10, 30, "Commands sent to", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(10, 42, "Raspberry Pi", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(10, 66, "KEY1: FLY MODE", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(10, 78, "KEY2: GYRO CAL", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(10, 90, "KEY4: BLUETOOTH", COLOR_YELLOW, COLOR_BLACK);

    printf("Command sender ready\n\n");

    // Input state
    InputState input_state = {0};
    ButtonReleaseTracker release_tracker = {0};

    // LED blink for visual feedback
    int led_blink_counter = 0;

    // Main loop
    while (true) {
        // Read inputs
        input_read(&input_state);

        // --- Button Press Events ---
        if (input_just_pressed_key1(&input_state)) {
            send_button_command(1, "PRESS");
            send_high_level_command("FLY_MODE");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "CMD: FLY_MODE", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
            release_tracker.key1_was_pressed = true;
        }

        if (input_just_pressed_key2(&input_state)) {
            send_button_command(2, "PRESS");
            send_high_level_command("GYRO_CALIBRATION");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "CMD: GYRO_CAL", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
            release_tracker.key2_was_pressed = true;
        }

        if (input_just_pressed_key4(&input_state)) {
            send_button_command(4, "PRESS");
            send_high_level_command("BLUETOOTH");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "CMD: BLUETOOTH", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
            release_tracker.key4_was_pressed = true;
        }

        if (input_just_pressed_press(&input_state)) {
            send_button_command(5, "PRESS");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "BTN: PRESS", COLOR_CYAN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
            release_tracker.press_was_pressed = true;
        }

        // --- Button Release Events ---
        if (release_tracker.key1_was_pressed && !input_state.key1) {
            send_button_command(1, "RELEASE");
            release_tracker.key1_was_pressed = false;
        }

        if (release_tracker.key2_was_pressed && !input_state.key2) {
            send_button_command(2, "RELEASE");
            release_tracker.key2_was_pressed = false;
        }

        if (release_tracker.key4_was_pressed && !input_state.key4) {
            send_button_command(4, "RELEASE");
            release_tracker.key4_was_pressed = false;
        }

        if (release_tracker.press_was_pressed && !input_state.press) {
            send_button_command(5, "RELEASE");
            release_tracker.press_was_pressed = false;
        }

        // --- Joystick Events ---
        if (input_just_pressed_up(&input_state)) {
            send_joystick_command("UP");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "JOY: UP", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
        }

        if (input_just_pressed_down(&input_state)) {
            send_joystick_command("DOWN");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "JOY: DOWN", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
        }

        if (input_just_pressed_left(&input_state)) {
            send_joystick_command("LEFT");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "JOY: LEFT", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
        }

        if (input_just_pressed_right(&input_state)) {
            send_joystick_command("RIGHT");
            lcd_fill_rect(10, 120, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 120, "JOY: RIGHT", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
            led_blink_counter = 5;
        }

        // Display current state at bottom
        char status[100];
        lcd_fill_rect(10, 160, 300, 12, COLOR_BLACK);
        snprintf(status, sizeof(status), "Joy: %s%s%s%s%s",
                 input_state.up ? "UP " : "",
                 input_state.down ? "DN " : "",
                 input_state.left ? "LF " : "",
                 input_state.right ? "RT " : "",
                 (!input_state.up && !input_state.down &&
                  !input_state.left && !input_state.right) ? "CENTER" : "");
        lcd_draw_string(10, 160, status, COLOR_WHITE, COLOR_BLACK);

        lcd_fill_rect(10, 180, 300, 12, COLOR_BLACK);
        snprintf(status, sizeof(status), "Btn: %s%s%s%s",
                 input_state.press ? "PRESS " : "",
                 input_state.key1 ? "K1 " : "",
                 input_state.key2 ? "K2 " : "",
                 input_state.key4 ? "K4 " : "");
        lcd_draw_string(10, 180, status, COLOR_WHITE, COLOR_BLACK);

        // LED blink management
        if (led_blink_counter > 0) {
            led_blink_counter--;
            if (led_blink_counter == 0) {
                gpio_put(LED_PIN, 0);
            }
        }

        // Small delay
        sleep_ms(10);
    }

    return 0;
}
