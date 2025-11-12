/**
 * Raspberry Pi Pico Command Receiver
 *
 * Receives button/joystick commands from Pico2 over USB CDC serial
 * and displays them on ST7789 LCD display.
 *
 * Features:
 * - Auto-reconnect on disconnect
 * - Low CPU usage with select()
 * - Console logging for verification
 * - Real-time LCD display updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include "serial_comm.h"
#include "pico_commands.h"
#include "st7789_rpi.h"

// Global flag for clean shutdown
static volatile sig_atomic_t running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

// Display state
typedef enum {
    DISPLAY_STATE_WAITING,      // Waiting for Pico connection
    DISPLAY_STATE_DISCONNECTED, // Pico disconnected
    DISPLAY_STATE_CONNECTED     // Showing received commands
} DisplayState;

// Draw the status screen (waiting or disconnected)
void draw_status_screen(DisplayState state) {
    lcd_clear(COLOR_BLACK);

    // Draw title
    lcd_draw_string_scaled(60, 20, "PICO RECEIVER", COLOR_CYAN, COLOR_BLACK, 2);

    // Draw status message based on state
    if (state == DISPLAY_STATE_WAITING) {
        lcd_draw_string_scaled(30, 80, "Waiting for", COLOR_YELLOW, COLOR_BLACK, 2);
        lcd_draw_string_scaled(60, 110, "Pico...", COLOR_YELLOW, COLOR_BLACK, 2);
    } else if (state == DISPLAY_STATE_DISCONNECTED) {
        lcd_draw_string_scaled(30, 80, "Disconnected", COLOR_RED, COLOR_BLACK, 2);
        lcd_draw_string_scaled(20, 110, "Reconnecting...", COLOR_RED, COLOR_BLACK, 2);
    }

    // Draw instructions at bottom
    lcd_draw_string(10, 200, "Press Ctrl+C to exit", COLOR_WHITE, COLOR_BLACK);
}

// Draw command screen with latest command
void draw_command_screen(const char* command_text) {
    // Clear previous command area (center of screen)
    lcd_fill_rect(0, 80, LCD_WIDTH, 80, COLOR_BLACK);

    // Calculate position for centered text
    // Each character is 6 pixels wide with scale 3 = 18 pixels per char
    size_t text_len = strlen(command_text);
    uint16_t text_width = text_len * 6 * 3;  // 6 pixels per char, scale 3
    uint16_t x_pos = (LCD_WIDTH - text_width) / 2;

    // Draw the command in large text
    lcd_draw_string_scaled(x_pos, 100, command_text, COLOR_GREEN, COLOR_BLACK, 3);
}

// Initialize the display
void init_display(void) {
    printf("Initializing LCD display...\n");
    if (lcd_init() != 0) {
        fprintf(stderr, "Failed to initialize LCD\n");
        exit(1);
    }
    printf("LCD initialized successfully\n");
}

int main(void) {
    int serial_fd = -1;
    char line_buffer[SERIAL_READ_BUFFER_SIZE];
    PicoCommand cmd;
    DisplayState display_state = DISPLAY_STATE_WAITING;
    time_t last_reconnect_attempt = 0;

    // Setup signal handler for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("=====================================\n");
    printf("  Raspberry Pi Pico Receiver v1.0\n");
    printf("  Press Ctrl+C to exit\n");
    printf("=====================================\n\n");

    // Initialize LCD display
    init_display();
    draw_status_screen(DISPLAY_STATE_WAITING);

    // Try to open serial port
    printf("Opening serial connection to Pico...\n");
    serial_fd = serial_init();

    if (serial_fd < 0) {
        printf("No Pico detected. Waiting for connection...\n");
    } else {
        printf("Connected to Pico on serial port\n");
        display_state = DISPLAY_STATE_CONNECTED;
        lcd_clear(COLOR_BLACK);
        lcd_draw_string_scaled(30, 20, "PICO RECEIVER", COLOR_CYAN, COLOR_BLACK, 2);
        lcd_draw_string(10, 60, "Waiting for commands...", COLOR_WHITE, COLOR_BLACK);
        lcd_draw_string(10, 200, "Press Ctrl+C to exit", COLOR_WHITE, COLOR_BLACK);
    }

    // Main loop
    while (running) {
        // Check if we need to reconnect
        if (serial_fd < 0) {
            time_t now = time(NULL);

            // Try to reconnect every 2 seconds
            if (now - last_reconnect_attempt >= 2) {
                if (display_state != DISPLAY_STATE_DISCONNECTED) {
                    display_state = DISPLAY_STATE_DISCONNECTED;
                    draw_status_screen(DISPLAY_STATE_DISCONNECTED);
                }

                serial_fd = serial_reconnect();
                last_reconnect_attempt = now;

                if (serial_fd >= 0) {
                    printf("Reconnected to Pico\n");
                    display_state = DISPLAY_STATE_CONNECTED;
                    lcd_clear(COLOR_BLACK);
                    lcd_draw_string_scaled(30, 20, "PICO RECEIVER", COLOR_CYAN, COLOR_BLACK, 2);
                    lcd_draw_string(10, 60, "Waiting for commands...", COLOR_WHITE, COLOR_BLACK);
                    lcd_draw_string(10, 200, "Press Ctrl+C to exit", COLOR_WHITE, COLOR_BLACK);
                }
            }

            // Sleep a bit before next check
            usleep(100000);  // 100ms
            continue;
        }

        // Use select() for efficient I/O with timeout
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(serial_fd, &readfds);

        // Timeout of 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(serial_fd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0) {
            // Error in select
            perror("select");
            break;
        }

        if (ret == 0) {
            // Timeout - no data available
            continue;
        }

        // Data available - read a line
        int bytes_read = serial_read_line(serial_fd, line_buffer, sizeof(line_buffer), 100);

        if (bytes_read < 0) {
            // Error or disconnection
            printf("Serial connection lost\n");
            serial_close(serial_fd);
            serial_fd = -1;
            display_state = DISPLAY_STATE_DISCONNECTED;
            draw_status_screen(DISPLAY_STATE_DISCONNECTED);
            continue;
        }

        if (bytes_read == 0) {
            // No data (timeout)
            continue;
        }

        // We received a line - print to console for verification
        printf("Received: %s\n", line_buffer);

        // Parse the command
        if (parse_pico_command(line_buffer, &cmd)) {
            // Valid command received

            // For CMD: type commands, display them prominently
            if (cmd.type == CMD_TYPE_CMD) {
                printf("  -> High-level command: %s\n", cmd.display_text);
                draw_command_screen(cmd.display_text);
            }
            // For button and joystick commands, just log and optionally display
            else if (cmd.type == CMD_TYPE_BTN) {
                printf("  -> Button event: %s\n", cmd.display_text);
                // Optionally update a status line
            }
            else if (cmd.type == CMD_TYPE_JOY) {
                printf("  -> Joystick event: %s\n", cmd.display_text);
                // Optionally update a status line
            }
        } else {
            printf("  -> Unknown command format\n");
        }
    }

    // Cleanup
    printf("\nShutting down...\n");

    if (serial_fd >= 0) {
        serial_close(serial_fd);
    }

    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(80, 100, "GOODBYE", COLOR_CYAN, COLOR_BLACK, 2);
    sleep(1);
    lcd_cleanup();

    printf("Cleanup complete\n");
    return 0;
}
