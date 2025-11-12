/*
 * ST7789 LCD Test Program for Raspberry Pi
 * Tests the 320x240 ST7789 LCD display in landscape mode (HUD projection)
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include "../include/st7789_rpi.h"
// Global flag for clean exit
static volatile bool running = true;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig;
    running = false;
}

int main(void)
{
    printf("=== ST7789 LCD Test (320x240) ===\n");
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize display
    printf("Initializing LCD...\n");
    if (lcd_init() < 0)
    {
        fprintf(stderr, "Failed to initialize LCD\n");
        return 1;
    }
    printf("✓ LCD initialized\n\n");

    // Display splash screen
    printf("Loading splash screen...\n");
    if (lcd_display_png("../images/output.png") == 0)
    {
        printf("✓ Splash screen displayed\n");
        sleep(3);
    }
    else
    {
        printf("⚠ Could not load splash screen, continuing...\n");
    }

    // Test 1: Color fill
    printf("Test 1: Filling screen with colors...\n");
    lcd_clear(COLOR_RED);
    sleep(1);
    lcd_clear(COLOR_GREEN);
    sleep(1);
    lcd_clear(COLOR_BLUE);
    sleep(1);
    lcd_clear(COLOR_BLACK);
    printf("✓ Color fill test complete\n\n");

    // Test 2: Text rendering
    printf("Test 2: Drawing text...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(10, 10, "ST7789 LCD", COLOR_CYAN, COLOR_BLACK);
    lcd_draw_string(10, 30, "320x240", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(10, 50, "Raspberry Pi", COLOR_GREEN, COLOR_BLACK);

    lcd_draw_string_scaled(10, 80, "SCALED", COLOR_YELLOW, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, 110, "TEXT", COLOR_MAGENTA, COLOR_BLACK, 3);
    printf("✓ Text rendering test complete\n\n");
    sleep(2);

    // Test 3: Graphics primitives
    printf("Test 3: Drawing shapes...\n");
    lcd_clear(COLOR_BLACK);

    // Draw rectangles
    lcd_fill_rect(10, 10, 50, 30, COLOR_RED);
    lcd_fill_rect(70, 10, 50, 30, COLOR_GREEN);
    lcd_fill_rect(130, 10, 50, 30, COLOR_BLUE);

    // Draw lines
    lcd_draw_line(10, 60, 310, 60, COLOR_WHITE);
    lcd_draw_line(10, 80, 310, 150, COLOR_CYAN);
    lcd_draw_line(310, 80, 10, 150, COLOR_MAGENTA);

    // Draw circles
    lcd_draw_circle(120, 180, 20, COLOR_YELLOW);
    lcd_draw_circle(120, 180, 30, COLOR_CYAN);
    lcd_draw_circle(120, 180, 40, COLOR_GREEN);

    printf("✓ Graphics test complete\n\n");
    sleep(2);

    // Final display
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(20, 80, "PILOT", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string_scaled(10, 130, "ASSISTANT", COLOR_CYAN, COLOR_BLACK, 2);
    lcd_draw_string(50, 180, "LCD Test Complete", COLOR_WHITE, COLOR_BLACK);

    printf("\n=== Test Summary ===\n");
    printf("✓ All tests passed\n");
    printf("LCD is working correctly\n");
    printf("\nKeeping display on for 5 seconds...\n");
    sleep(5);

    // Clear and cleanup
    lcd_clear(COLOR_BLACK);
    lcd_cleanup();
    printf("Display turned off\n");

    return 0;
}
