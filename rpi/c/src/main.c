/**
 * Pilot Assistant - Main Application
 * Raspberry Pi C Implementation with ST7789 LCD
 *
 * Receives input from Pico2 via USB serial (/dev/ttyACM0)
 * - Pico sends: BTN:<button_name>:PRESSED/RELEASED
 * - Buttons: up, down, left, right, press, key1, key2, key4
 *
 * Hardware:
 * - 320x240 ST7789 LCD (landscape mode)
 * - Pico2 connected via USB (handles joystick and buttons)
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "../include/st7789_rpi.h"

// Serial configuration
#define PICO_DEVICE "/dev/ttyACM0"
#define BAUD_RATE B115200
#define BUFFER_SIZE 256

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
static int serial_fd = -1;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig;
    running = false;
}

/**
 * Initialize serial port for Pico communication
 */
static int serial_init(const char *device)
{
    int fd;
    struct termios options;

    // Open serial port
    fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Failed to open serial device");
        return -1;
    }

    // Get current options
    if (tcgetattr(fd, &options) < 0) {
        perror("Failed to get serial attributes");
        close(fd);
        return -1;
    }

    // Set baud rate
    cfsetispeed(&options, BAUD_RATE);
    cfsetospeed(&options, BAUD_RATE);

    // Configure for raw input
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL);
    options.c_oflag &= ~OPOST;

    // Set timeout (0.1 second)
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    // Apply options
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("Failed to set serial attributes");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
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
 * Placeholder menu functions
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
 * Parse button message from Pico
 * Format: BTN:<button_name>:PRESSED or BTN:<button_name>:RELEASED
 * Returns: button name string, or NULL if not a valid button press
 */
const char* parse_button_press(const char *message)
{
    static char button_name[32];

    // Check if it starts with "BTN:" and ends with ":PRESSED"
    if (strncmp(message, "BTN:", 4) != 0) {
        return NULL;
    }

    const char *end = strstr(message, ":PRESSED");
    if (!end) {
        return NULL;
    }

    // Extract button name
    int name_len = end - (message + 4);
    if (name_len <= 0 || name_len >= 32) {
        return NULL;
    }

    strncpy(button_name, message + 4, name_len);
    button_name[name_len] = '\0';

    return button_name;
}

/**
 * Process serial input from Pico
 */
void process_serial_input(MenuOption *current_selection, bool *menu_changed)
{
    static char buffer[BUFFER_SIZE];
    static int buf_pos = 0;
    char c;

    *menu_changed = false;

    while (read(serial_fd, &c, 1) > 0) {
        if (c == '\n' || c == '\r') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';

                // Parse button press
                const char *button = parse_button_press(buffer);
                if (button) {
                    printf("Button: %s\n", button);

                    // Handle navigation
                    if (strcmp(button, "down") == 0) {
                        *current_selection = (*current_selection + 1) % MENU_ITEM_COUNT;
                        *menu_changed = true;
                    }
                    else if (strcmp(button, "up") == 0) {
                        *current_selection = (*current_selection - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
                        *menu_changed = true;
                    }
                    // Handle selection (right, press, or key1)
                    else if (strcmp(button, "right") == 0 ||
                             strcmp(button, "press") == 0 ||
                             strcmp(button, "key1") == 0) {

                        // Execute menu action
                        switch (*current_selection) {
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

                        *menu_changed = true;
                    }
                    // Exit on key4
                    else if (strcmp(button, "key4") == 0) {
                        printf("Exit requested\n");
                        running = false;
                    }
                }

                buf_pos = 0;
            }
        }
        else if (buf_pos < BUFFER_SIZE - 1) {
            buffer[buf_pos++] = c;
        }
        else {
            // Buffer overflow - reset
            buf_pos = 0;
        }
    }
}

/**
 * Main application
 */
int main(void)
{
    printf("=== Pilot Assistant ===\n");
    printf("Raspberry Pi C Implementation\n");
    printf("Pico Device: %s\n", PICO_DEVICE);
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

    // Initialize serial connection to Pico
    printf("Connecting to Pico...\n");
    serial_fd = serial_init(PICO_DEVICE);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial connection to Pico\n");
        fprintf(stderr, "Make sure Pico is connected to USB\n");
        lcd_clear(COLOR_BLACK);
        lcd_draw_string(40, 100, "ERROR:", COLOR_RED, COLOR_BLACK);
        lcd_draw_string(20, 120, "Pico not connected", COLOR_WHITE, COLOR_BLACK);
        sleep(3);
        lcd_cleanup();
        return 1;
    }
    printf("✓ Connected to Pico\n\n");

    // Menu state
    MenuOption current_selection = MENU_GYRO;
    bool menu_changed = true;

    // Main loop
    printf("=== Main Menu Active ===\n");
    while (running) {
        // Draw menu if changed
        if (menu_changed) {
            draw_menu(current_selection);
            menu_changed = false;
        }

        // Process input from Pico
        process_serial_input(&current_selection, &menu_changed);

        // Small delay to avoid CPU spinning
        usleep(10000); // 10ms
    }

    // Cleanup
    printf("\nShutting down...\n");
    lcd_clear(COLOR_BLACK);
    if (serial_fd >= 0) {
        close(serial_fd);
    }
    lcd_cleanup();
    printf("✓ Cleanup complete\n");

    return 0;
}
