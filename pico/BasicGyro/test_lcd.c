/**
 * Simple LCD test - just draw something on screen
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "st7789_lcd.h"

int main(void) {
    stdio_init_all();
    sleep_ms(1000);

    printf("LCD Test starting...\n");

    // Initialize LCD
    lcd_init();
    printf("LCD initialized\n");

    // Clear screen to red
    lcd_clear(COLOR_RED);
    lcd_flush();
    printf("Screen cleared to RED\n");

    sleep_ms(1000);

    // Draw some text
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(10, 10, "LCD TEST", COLOR_WHITE, COLOR_BLACK, 3);
    lcd_draw_string_scaled(10, 50, "If you see this", COLOR_GREEN, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, 80, "LCD is working!", COLOR_YELLOW, COLOR_BLACK, 2);
    lcd_flush();
    printf("Text drawn\n");

    // Flash colors
    while (1) {
        sleep_ms(1000);
        lcd_clear(COLOR_RED);
        lcd_flush();
        printf("RED\n");

        sleep_ms(1000);
        lcd_clear(COLOR_GREEN);
        lcd_flush();
        printf("GREEN\n");

        sleep_ms(1000);
        lcd_clear(COLOR_BLUE);
        lcd_flush();
        printf("BLUE\n");
    }

    return 0;
}
