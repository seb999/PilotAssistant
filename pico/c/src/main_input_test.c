/**
 * Input Handler Test Program
 * Tests joystick and button input with visual feedback on LCD
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "input_handler.h"
#include "st7789_lcd.h"
#include "splash_data.h"

#define LED_PIN 25

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
    printf("  Input Handler Test v1.0\n");
    printf("  Testing Joystick & Buttons\n");
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
    lcd_draw_string(10, 10, "INPUT TEST", COLOR_CYAN, COLOR_BLACK);
    lcd_draw_string(10, 30, "Move joystick", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(10, 42, "Press buttons", COLOR_WHITE, COLOR_BLACK);

    printf("Input handler initialized\n");
    printf("Ready to test inputs...\n\n");

    // Input state
    InputState input_state = {0};

    // LED blink counter
    int led_state = 0;

    // Main loop
    while (true) {
        // Read inputs
        input_read(&input_state);

        // Check for button presses and display on LCD
        if (input_just_pressed_up(&input_state)) {
            printf("UP pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "UP pressed", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_down(&input_state)) {
            printf("DOWN pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "DOWN pressed", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_left(&input_state)) {
            printf("LEFT pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "LEFT pressed", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_right(&input_state)) {
            printf("RIGHT pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "RIGHT pressed", COLOR_YELLOW, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_press(&input_state)) {
            printf("PRESS pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "PRESS pressed", COLOR_CYAN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_key1(&input_state)) {
            printf("KEY1 pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "KEY1 pressed", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_key2(&input_state)) {
            printf("KEY2 pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "KEY2 pressed", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        if (input_just_pressed_key4(&input_state)) {
            printf("KEY4 pressed\n");
            lcd_fill_rect(10, 60, 300, 12, COLOR_BLACK);
            lcd_draw_string(10, 60, "KEY4 pressed", COLOR_GREEN, COLOR_BLACK);
            gpio_put(LED_PIN, 1);
        }

        // Display current state
        char status[100];

        // Joystick state
        lcd_fill_rect(10, 80, 300, 12, COLOR_BLACK);
        snprintf(status, sizeof(status), "Joy: %s%s%s%s%s",
                 input_state.up ? "UP " : "",
                 input_state.down ? "DN " : "",
                 input_state.left ? "LF " : "",
                 input_state.right ? "RT " : "",
                 (!input_state.up && !input_state.down &&
                  !input_state.left && !input_state.right) ? "CENTER" : "");
        lcd_draw_string(10, 80, status, COLOR_WHITE, COLOR_BLACK);

        // Button state
        lcd_fill_rect(10, 100, 300, 12, COLOR_BLACK);
        snprintf(status, sizeof(status), "Btn: %s%s%s%s",
                 input_state.press ? "PRESS " : "",
                 input_state.key1 ? "K1 " : "",
                 input_state.key2 ? "K2 " : "",
                 input_state.key4 ? "K4 " : "");
        lcd_draw_string(10, 100, status, COLOR_WHITE, COLOR_BLACK);

        // Turn off LED after a short time
        if (gpio_get(LED_PIN)) {
            sleep_ms(50);
            gpio_put(LED_PIN, 0);
        }

        // Small delay
        sleep_ms(10);
    }

    return 0;
}
