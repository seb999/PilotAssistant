/*
 * Menu HUD Display for Raspberry Pi
 * Receives menu state from Pico via USB serial and displays on LCD
 * Optimized for HUD projection (white text on black background)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "st7789_rpi.h"

// Serial configuration
#define PICO_DEVICE "/dev/ttyACM0"
#define BAUD_RATE   B115200
#define BUFFER_SIZE 256

// Menu configuration
#define MENU_ITEMS_MAX 4
#define MENU_ITEM_HEIGHT 50
#define MENU_START_Y 30

// HUD colors (bright on black for projection)
#define HUD_BG_COLOR       COLOR_BLACK
#define HUD_TEXT_COLOR     COLOR_WHITE
#define HUD_SELECTED_COLOR COLOR_CYAN
#define HUD_TITLE_COLOR    COLOR_CYAN

// Menu item labels (must match Pico order)
static const char* menu_labels[] = {
    "GO FLY",
    "BLUETOOTH",
    "GYRO OFFSET",
    "RADAR"
};

// Global state
static int serial_fd = -1;
static volatile bool running = true;
static int current_selection = 0;
static int menu_item_count = 4;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

// Initialize serial port
static int serial_init(const char* device) {
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

// Draw splash screen (HUD optimized)
static void draw_splash(void) {
    lcd_clear(HUD_BG_COLOR);

    // Large title
    lcd_draw_string_scaled(30, 60, "PILOT", HUD_TITLE_COLOR, HUD_BG_COLOR, 3);
    lcd_draw_string_scaled(20, 110, "ASSISTANT", HUD_TITLE_COLOR, HUD_BG_COLOR, 2);

    // HUD indicator
    lcd_draw_string(70, 160, "HUD MODE", HUD_TEXT_COLOR, HUD_BG_COLOR);
    lcd_draw_string(40, 180, "Waiting for Pico...", HUD_TEXT_COLOR, HUD_BG_COLOR);
}

// Draw single menu item (HUD optimized - large, bright text)
static void draw_menu_item(int index, bool selected) {
    if (index < 0 || index >= menu_item_count) return;

    uint16_t y = MENU_START_Y + (index * MENU_ITEM_HEIGHT);
    uint16_t color = selected ? HUD_SELECTED_COLOR : HUD_TEXT_COLOR;
    uint8_t scale = selected ? 3 : 2;

    // Clear area
    lcd_fill_rect(10, y, 220, MENU_ITEM_HEIGHT - 5, HUD_BG_COLOR);

    // Draw label (large for HUD readability)
    lcd_draw_string_scaled(20, y + 10, menu_labels[index], color, HUD_BG_COLOR, scale);

    // Draw selection indicator
    if (selected) {
        // Arrow indicator
        lcd_draw_string_scaled(5, y + 10, ">", HUD_SELECTED_COLOR, HUD_BG_COLOR, scale);

        // Underline
        lcd_draw_line(20, y + MENU_ITEM_HEIGHT - 10,
                     220, y + MENU_ITEM_HEIGHT - 10, HUD_SELECTED_COLOR);
    }
}

// Draw full menu
static void draw_menu(void) {
    lcd_clear(HUD_BG_COLOR);

    // Draw title bar
    lcd_draw_string_scaled(50, 5, "MENU", HUD_TITLE_COLOR, HUD_BG_COLOR, 2);
    lcd_draw_line(0, 25, LCD_WIDTH, 25, HUD_TITLE_COLOR);

    // Draw all menu items
    for (int i = 0; i < menu_item_count; i++) {
        draw_menu_item(i, i == current_selection);
    }

    // Draw footer
    lcd_draw_line(0, 215, LCD_WIDTH, 215, HUD_TITLE_COLOR);
    lcd_draw_string(60, 222, "HUD DISPLAY", HUD_TEXT_COLOR, HUD_BG_COLOR);
}

// Update menu selection
static void update_selection(int old_selection, int new_selection) {
    if (old_selection >= 0 && old_selection < menu_item_count) {
        draw_menu_item(old_selection, false);
    }
    if (new_selection >= 0 && new_selection < menu_item_count) {
        draw_menu_item(new_selection, true);
    }
}

// Simple JSON parser (extract selected index)
static bool parse_menu_message(const char* json, int* selected, int* total) {
    // Look for "selected":N pattern
    const char* sel_ptr = strstr(json, "\"selected\":");
    if (sel_ptr) {
        sel_ptr += 11; // Skip "selected":
        *selected = atoi(sel_ptr);
    } else {
        return false;
    }

    // Look for "total":N pattern
    const char* total_ptr = strstr(json, "\"total\":");
    if (total_ptr) {
        total_ptr += 8; // Skip "total":
        *total = atoi(total_ptr);
    } else {
        *total = MENU_ITEMS_MAX;
    }

    return true;
}

// Check message type
static const char* get_message_type(const char* json) {
    const char* type_ptr = strstr(json, "\"type\":\"");
    if (type_ptr) {
        type_ptr += 8; // Skip "type":"
        if (strncmp(type_ptr, "splash", 6) == 0) return "splash";
        if (strncmp(type_ptr, "menu", 4) == 0) return "menu";
        if (strncmp(type_ptr, "action", 6) == 0) return "action";
    }
    return NULL;
}

// Read and process serial messages
static void process_serial_input(void) {
    static char buffer[BUFFER_SIZE];
    static int buf_pos = 0;
    char c;

    while (read(serial_fd, &c, 1) > 0) {
        if (c == '\n' || c == '\r') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';

                // Debug output
                printf("Received: %s\n", buffer);

                // Parse message
                const char* msg_type = get_message_type(buffer);

                if (msg_type) {
                    if (strcmp(msg_type, "splash") == 0) {
                        draw_splash();
                    }
                    else if (strcmp(msg_type, "menu") == 0) {
                        int selected = 0, total = MENU_ITEMS_MAX;
                        if (parse_menu_message(buffer, &selected, &total)) {
                            // Clamp to valid range
                            if (selected < 0) selected = 0;
                            if (selected >= total) selected = total - 1;

                            int old_selection = current_selection;
                            current_selection = selected;
                            menu_item_count = total;

                            // First menu message - draw full menu
                            static bool first_menu = true;
                            if (first_menu) {
                                draw_menu();
                                first_menu = false;
                            } else {
                                // Update only changed items
                                update_selection(old_selection, current_selection);
                            }

                            printf("Menu: %s selected\n", menu_labels[selected]);
                        }
                    }
                    else if (strcmp(msg_type, "action") == 0) {
                        // Action selected - could show confirmation or action screen
                        printf("Action: %s\n", menu_labels[current_selection]);

                        // Flash selection
                        lcd_fill_rect(0, MENU_START_Y + (current_selection * MENU_ITEM_HEIGHT),
                                     LCD_WIDTH, MENU_ITEM_HEIGHT - 5, HUD_SELECTED_COLOR);
                        usleep(100000);
                        draw_menu_item(current_selection, true);
                    }
                }

                buf_pos = 0;
            }
        } else if (buf_pos < BUFFER_SIZE - 1) {
            buffer[buf_pos++] = c;
        } else {
            // Buffer overflow - reset
            buf_pos = 0;
        }
    }
}

int main(void) {
    printf("=== Pilot Assistant HUD Display ===\n");
    printf("Pico Device: %s\n", PICO_DEVICE);
    printf("Baud Rate: 115200\n");
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize LCD
    printf("Initializing LCD (HUD mode)...\n");
    if (lcd_init() < 0) {
        fprintf(stderr, "Failed to initialize LCD\n");
        return 1;
    }
    printf("✓ LCD initialized\n");

    // Show initial splash
    draw_splash();

    // Initialize serial connection to Pico
    printf("Connecting to Pico...\n");
    serial_fd = serial_init(PICO_DEVICE);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial connection to Pico\n");
        fprintf(stderr, "Make sure Pico is connected to USB\n");
        lcd_clear(HUD_BG_COLOR);
        lcd_draw_string(40, 100, "ERROR:", COLOR_RED, HUD_BG_COLOR);
        lcd_draw_string(20, 120, "Pico not connected", HUD_TEXT_COLOR, HUD_BG_COLOR);
        sleep(3);
        lcd_cleanup();
        return 1;
    }
    printf("✓ Connected to Pico\n\n");

    // Main loop
    printf("Waiting for menu data from Pico...\n");
    while (running) {
        process_serial_input();
        usleep(10000); // 10ms sleep to avoid CPU spinning
    }

    // Cleanup
    printf("\n\nShutting down...\n");
    lcd_clear(HUD_BG_COLOR);
    lcd_draw_string(60, 110, "GOODBYE", HUD_TEXT_COLOR, HUD_BG_COLOR);
    sleep(1);

    if (serial_fd >= 0) {
        close(serial_fd);
    }
    lcd_cleanup();

    printf("HUD display stopped\n");
    return 0;
}
