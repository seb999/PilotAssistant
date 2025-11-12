/**
 * Pilot Assistant - Test Version (without Pico)
 * Raspberry Pi C Implementation with ST7789 LCD
 *
 * This version uses keyboard input for testing without Pico hardware
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include "../include/st7789_rpi.h"

// Menu system
#define MENU_ITEM_COUNT 6

typedef enum {
    MENU_GYRO = 0,
    MENU_CAMERA,
    MENU_GPS,
    MENU_TRAFFIC,
    MENU_BLUETOOTH,
    MENU_FLY
} MenuOption;

// Global variables
static volatile bool running = true;
static struct termios orig_termios;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig;
    running = false;
}

// Restore terminal settings on exit
void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Enable raw mode for keyboard input
void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/**
 * Draw the main menu
 */
void draw_menu(MenuOption selected)
{
    const char *menu_items[MENU_ITEM_COUNT] = {
        "1. GYRO",
        "2. AI-CAMERA",
        "3. GPS",
        "4. TRAFFIC",
        "5. BLUETOOTH",
        "6. GO FLY"
    };

    lcd_clear(COLOR_BLACK);

    // Title
    lcd_draw_string_scaled(60, 20, "PILOT", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string_scaled(40, 55, "ASSISTANT", COLOR_CYAN, COLOR_BLACK, 2);

    // Draw horizontal line
    lcd_draw_line(10, 90, 310, 90, COLOR_CYAN);

    // Menu items
    uint16_t y_pos = 105;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        uint16_t color = ((MenuOption)i == selected) ? COLOR_CYAN : COLOR_WHITE;

        // Draw selection indicator
        if ((MenuOption)i == selected) {
            lcd_draw_string(10, y_pos, ">", COLOR_CYAN, COLOR_BLACK);
        }

        lcd_draw_string(25, y_pos, menu_items[i], color, COLOR_BLACK);
        y_pos += 20;
    }
}

/**
 * Menu functions
 */
void menu_gyro(void)
{
    printf("Opening Gyro menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(70, 100, "GYRO", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(80, 140, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

void menu_camera(void)
{
    printf("Opening AI-Camera menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(40, 100, "CAMERA", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(80, 140, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

void menu_gps(void)
{
    printf("Opening GPS menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(80, 100, "GPS", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(80, 140, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

void menu_traffic(void)
{
    printf("Opening Traffic menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(40, 100, "TRAFFIC", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(80, 140, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

void menu_bluetooth(void)
{
    printf("Opening Bluetooth menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(20, 100, "BLUETOOTH", COLOR_CYAN, COLOR_BLACK, 2);
    lcd_draw_string(80, 140, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

void menu_fly(void)
{
    printf("Opening Go FLY menu...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(40, 90, "GO FLY", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(60, 130, "Flight Display", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(80, 150, "Coming soon...", COLOR_WHITE, COLOR_BLACK);
    sleep(2);
}

/**
 * Main application
 */
int main(void)
{
    printf("=== Pilot Assistant - TEST MODE ===\n");
    printf("Raspberry Pi C Implementation\n");
    printf("Using keyboard for input (no Pico required)\n");
    printf("Controls: W/S=Up/Down, SPACE=Select, Q=Quit\n");
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize display
    printf("Initializing LCD...\n");
    if (lcd_init() < 0) {
        fprintf(stderr, "Failed to initialize LCD\n");
        return 1;
    }
    printf("✓ LCD initialized\n");

    // Display splash screen
    printf("Loading splash screen...\n");
    if (lcd_display_png("../images/output.png") == 0) {
        printf("✓ Splash screen displayed\n");
        sleep(2);
    } else {
        printf("⚠ Could not load splash screen, continuing...\n");
    }

    // Enable raw keyboard input
    enable_raw_mode();

    // Menu state
    MenuOption current_selection = MENU_GYRO;
    bool menu_changed = true;

    // Main loop
    printf("\n=== Main Menu Active ===\n");
    while (running) {
        // Draw menu if changed
        if (menu_changed) {
            draw_menu(current_selection);
            menu_changed = false;
        }

        // Read keyboard input
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            switch (c) {
                case 'w':
                case 'W':
                    current_selection = (current_selection - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
                    menu_changed = true;
                    printf("UP -> %d\n", current_selection);
                    break;

                case 's':
                case 'S':
                    current_selection = (current_selection + 1) % MENU_ITEM_COUNT;
                    menu_changed = true;
                    printf("DOWN -> %d\n", current_selection);
                    break;

                case ' ':
                case '\n':
                case '\r':
                    printf("SELECT -> %d\n", current_selection);
                    switch (current_selection) {
                        case MENU_GYRO:
                            menu_gyro();
                            break;
                        case MENU_CAMERA:
                            menu_camera();
                            break;
                        case MENU_GPS:
                            menu_gps();
                            break;
                        case MENU_TRAFFIC:
                            menu_traffic();
                            break;
                        case MENU_BLUETOOTH:
                            menu_bluetooth();
                            break;
                        case MENU_FLY:
                            menu_fly();
                            break;
                    }
                    menu_changed = true;
                    break;

                case 'q':
                case 'Q':
                case 'x':
                case 'X':
                    printf("Exit requested\n");
                    running = false;
                    break;
            }
        }

        // Small delay to avoid CPU spinning
        usleep(10000); // 10ms
    }

    // Cleanup
    printf("\nShutting down...\n");
    lcd_clear(COLOR_BLACK);
    lcd_cleanup();
    printf("✓ Cleanup complete\n");

    return 0;
}
