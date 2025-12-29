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

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include "../include/st7789_rpi.h"
#include "../include/adxl345.h"
#include "../include/gps.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Serial configuration
#define PICO_DEVICE "/dev/ttyACM0"
#define BAUD_RATE B115200
#define BUFFER_SIZE 256

// Attitude indicator configuration
#define SCREEN_CENTER_X (LCD_WIDTH / 2)
#define SCREEN_CENTER_Y (LCD_HEIGHT / 2)
#define HORIZON_BAR_WIDTH LCD_WIDTH  // Full screen width
#define HORIZON_BAR_HEIGHT 4
#define PITCH_SCALE 2  // pixels per degree
#define AIRCRAFT_SYMBOL_SIZE 40

// Speed/Altitude tape configuration
#define TAPE_WIDTH 15
#define TAPE_HEIGHT LCD_HEIGHT  // Full screen height
#define TAPE_MARGIN 5

// Global variables
static volatile bool running = true;
static int serial_fd = -1;
static int adxl345_fd = -1;
static int gps_fd = -1;

// Attitude data
typedef struct {
    float pitch;  // degrees (positive = nose up)
    float roll;   // degrees (positive = right wing down)
} AttitudeData;

static AttitudeData attitude = {0.0f, 0.0f};
static AttitudeData last_drawn_attitude = {999.0f, 999.0f};  // Force initial draw

// No filtering - match Python implementation for maximum responsiveness

// Sensor update rate - match Python (sleep 0.1 = 100ms, but we go faster)
#define SENSOR_UPDATE_MS 50  // 20 Hz like Python attitude_service
#define GPS_UPDATE_MS 200    // 5 Hz update rate

// GPS data
static GPSData gps_data = {0.0f, 0.0f, false, 0};

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
 * Draw pitch ladder lines - rotated with roll
 */
void draw_pitch_ladder(float pitch, float roll)
{
    // Convert roll to radians
    float roll_rad = roll * M_PI / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);

    // Draw pitch lines at 10 degree intervals
    for (int pitch_angle = -30; pitch_angle <= 30; pitch_angle += 10) {
        if (pitch_angle == 0) continue; // Skip horizon (drawn separately)

        int y_offset = (int)((pitch_angle - pitch) * PITCH_SCALE);

        // Line length based on angle
        int line_len = (pitch_angle % 20 == 0) ? 30 : 20;
        uint16_t color = (pitch_angle > 0) ? COLOR_CYAN : COLOR_WHITE;

        // Calculate rotated endpoints
        int x1 = SCREEN_CENTER_X + (int)(-line_len * cos_roll - y_offset * sin_roll);
        int y1 = SCREEN_CENTER_Y + (int)(-line_len * sin_roll + y_offset * cos_roll);
        int x2 = SCREEN_CENTER_X + (int)(line_len * cos_roll - y_offset * sin_roll);
        int y2 = SCREEN_CENTER_Y + (int)(line_len * sin_roll + y_offset * cos_roll);

        // Only draw if at least one point is visible on screen
        if ((x1 >= 0 && x1 < LCD_WIDTH && y1 >= 0 && y1 < LCD_HEIGHT) ||
            (x2 >= 0 && x2 < LCD_WIDTH && y2 >= 0 && y2 < LCD_HEIGHT)) {

            lcd_fb_draw_line(x1, y1, x2, y2, color);

            // Draw pitch value for major lines (skip text rotation for now - too complex)
            if (pitch_angle % 20 == 0 && fabs(roll) < 45.0f) {
                char pitch_str[8];
                snprintf(pitch_str, sizeof(pitch_str), "%d", pitch_angle > 0 ? pitch_angle : -pitch_angle);

                // Position text near the line endpoints
                int text_x1 = x1 - 15;
                int text_y1 = y1 - 3;
                int text_x2 = x2 + 5;
                int text_y2 = y2 - 3;

                if (text_x1 >= 0 && text_x1 < LCD_WIDTH - 20 && text_y1 >= 0 && text_y1 < LCD_HEIGHT - 10) {
                    lcd_fb_draw_string(text_x1, text_y1, pitch_str, color, COLOR_BLACK);
                }
                if (text_x2 >= 0 && text_x2 < LCD_WIDTH - 20 && text_y2 >= 0 && text_y2 < LCD_HEIGHT - 10) {
                    lcd_fb_draw_string(text_x2, text_y2, pitch_str, color, COLOR_BLACK);
                }
            }
        }
    }
}

/**
 * Draw the horizon bar (cyan) - rotated based on roll angle
 */
void draw_horizon(float pitch, float roll)
{
    // Calculate horizon position based on pitch
    int y_offset = (int)(pitch * PITCH_SCALE);
    int horizon_y = SCREEN_CENTER_Y - y_offset;

    // Convert roll to radians
    float roll_rad = roll * M_PI / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);

    // Draw thick cyan rotated bar for horizon
    int half_width = HORIZON_BAR_WIDTH / 2;

    for (int i = 0; i < HORIZON_BAR_HEIGHT; i++) {
        // Calculate offset from center line
        int offset_y = i - HORIZON_BAR_HEIGHT / 2;

        // Calculate rotated endpoints
        int x1 = SCREEN_CENTER_X + (int)(-half_width * cos_roll - offset_y * sin_roll);
        int y1 = horizon_y + (int)(-half_width * sin_roll + offset_y * cos_roll);
        int x2 = SCREEN_CENTER_X + (int)(half_width * cos_roll - offset_y * sin_roll);
        int y2 = horizon_y + (int)(half_width * sin_roll + offset_y * cos_roll);

        lcd_fb_draw_line(x1, y1, x2, y2, COLOR_CYAN);
    }
}

/**
 * Draw aircraft symbol (static in center)
 */
void draw_aircraft_symbol(void)
{
    uint16_t cx = SCREEN_CENTER_X;
    uint16_t cy = SCREEN_CENTER_Y;

    // Draw center dot
    lcd_fb_fill_rect(cx - 2, cy - 2, 5, 5, COLOR_YELLOW);

    // Draw wings (horizontal lines)
    lcd_fb_draw_line(cx - AIRCRAFT_SYMBOL_SIZE, cy, cx - 10, cy, COLOR_YELLOW);
    lcd_fb_draw_line(cx + 10, cy, cx + AIRCRAFT_SYMBOL_SIZE, cy, COLOR_YELLOW);
    lcd_fb_draw_line(cx - AIRCRAFT_SYMBOL_SIZE, cy + 1, cx - 10, cy + 1, COLOR_YELLOW);
    lcd_fb_draw_line(cx + 10, cy + 1, cx + AIRCRAFT_SYMBOL_SIZE, cy + 1, COLOR_YELLOW);

    // Draw wing tips (vertical marks)
    lcd_fb_draw_line(cx - AIRCRAFT_SYMBOL_SIZE, cy - 5, cx - AIRCRAFT_SYMBOL_SIZE, cy + 5, COLOR_YELLOW);
    lcd_fb_draw_line(cx + AIRCRAFT_SYMBOL_SIZE, cy - 5, cx + AIRCRAFT_SYMBOL_SIZE, cy + 5, COLOR_YELLOW);
}

/**
 * Draw roll indicator (bank angle arc at top)
 */
void draw_roll_indicator(float roll)
{
    uint16_t cx = SCREEN_CENTER_X;
    uint16_t top_y = 30;

    // Draw roll scale marks
    for (int angle = -60; angle <= 60; angle += 30) {
        int tick_len = (angle == 0) ? 12 : 8;
        // Simple vertical tick marks (proper arc calculation could be added)
        int x = cx + (angle * 2);
        lcd_fb_draw_line(x, top_y, x, top_y + tick_len, COLOR_WHITE);
    }

    // Draw current roll indicator (triangle pointing to current bank angle)
    int roll_x = cx + (int)(roll * 2);
    lcd_fb_draw_line(roll_x - 4, top_y + 15, roll_x, top_y + 10, COLOR_YELLOW);
    lcd_fb_draw_line(roll_x + 4, top_y + 15, roll_x, top_y + 10, COLOR_YELLOW);
    lcd_fb_draw_line(roll_x - 4, top_y + 15, roll_x + 4, top_y + 15, COLOR_YELLOW);
}

/**
 * Draw speed tape on right side (knots)
 */
void draw_speed_tape(float speed_knots)
{
    // Position tape at right edge of screen, full height
    uint16_t x = LCD_WIDTH - TAPE_WIDTH;
    uint16_t y = 0;

    // Draw tape background
    lcd_fb_fill_rect(x, y, TAPE_WIDTH, TAPE_HEIGHT, COLOR_BLACK);

    // Draw left border only
    lcd_fb_draw_line(x, y, x, y + TAPE_HEIGHT, COLOR_WHITE);

    // Draw speed marks (5 knot increments) - just tick marks, no text
    int base_speed = ((int)speed_knots / 5) * 5;  // Round to nearest 5

    for (int i = -50; i <= 50; i += 5) {
        int mark_speed = base_speed + i;
        if (mark_speed < 0) continue;

        float offset = (mark_speed - speed_knots) * 3;  // 3 pixels per knot
        int mark_y = SCREEN_CENTER_Y + (int)offset;

        if (mark_y >= 0 && mark_y < LCD_HEIGHT) {
            // Draw tick mark - longer for 10 knot increments
            int tick_len = (mark_speed % 10 == 0) ? 8 : 5;
            lcd_fb_draw_line(x, mark_y, x + tick_len, mark_y, COLOR_WHITE);
        }
    }

    // No yellow box - just the tape with tick marks
}

/**
 * Draw altitude tape on left side (meters)
 */
void draw_altitude_tape(float altitude_meters)
{
    // Position tape at left edge of screen, full height
    uint16_t x = 0;
    uint16_t y = 0;

    // Draw tape background
    lcd_fb_fill_rect(x, y, TAPE_WIDTH, TAPE_HEIGHT, COLOR_BLACK);

    // Draw right border only
    lcd_fb_draw_line(x + TAPE_WIDTH, y, x + TAPE_WIDTH, y + TAPE_HEIGHT, COLOR_WHITE);

    // Draw altitude marks (10 meter increments) - just tick marks, no text
    int base_alt = ((int)altitude_meters / 10) * 10;  // Round to nearest 10

    for (int i = -100; i <= 100; i += 10) {
        int mark_alt = base_alt + i;

        float offset = (mark_alt - altitude_meters) * 2;  // 2 pixels per meter
        int mark_y = SCREEN_CENTER_Y + (int)offset;

        if (mark_y >= 0 && mark_y < LCD_HEIGHT) {
            // Draw tick mark - longer for 20 meter increments
            int tick_len = (mark_alt % 20 == 0) ? 8 : 5;
            lcd_fb_draw_line(x + TAPE_WIDTH - tick_len, mark_y, x + TAPE_WIDTH, mark_y, COLOR_WHITE);
        }
    }

    // No yellow box - just the tape with tick marks
}

/**
 * Draw complete attitude indicator
 * Uses framebuffer for smooth, flicker-free updates
 */
void draw_attitude_indicator(void)
{
    // Clear framebuffer to black
    lcd_fb_clear(COLOR_BLACK);

    // Draw components in order (back to front) to framebuffer
    draw_pitch_ladder(attitude.pitch, attitude.roll);
    draw_horizon(attitude.pitch, attitude.roll);
    draw_speed_tape(gps_data.speed_knots);
    draw_altitude_tape(gps_data.altitude_meters);
    draw_aircraft_symbol();
    draw_roll_indicator(attitude.roll);

    // Draw GPS fix status
    if (gps_data.has_fix) {
        lcd_fb_draw_string(SCREEN_CENTER_X - 15, 10, "GPS", COLOR_GREEN, COLOR_BLACK);
    } else {
        lcd_fb_draw_string(SCREEN_CENTER_X - 20, 10, "NO GPS", COLOR_RED, COLOR_BLACK);
    }

    // Draw pitch warning if exceeding ±20 degrees
    if (fabs(attitude.pitch) > 20.0f) {
        // Red warning box at bottom center
        int warning_width = 120;
        int warning_height = 30;
        int warning_x = SCREEN_CENTER_X - warning_width / 2;
        int warning_y = LCD_HEIGHT - warning_height - 10;

        // Draw red background box
        lcd_fb_fill_rect(warning_x, warning_y, warning_width, warning_height, COLOR_RED);

        // Draw warning text
        char warning_text[20];
        if (attitude.pitch > 20.0f) {
            snprintf(warning_text, sizeof(warning_text), "PITCH UP %.0f", attitude.pitch);
        } else {
            snprintf(warning_text, sizeof(warning_text), "PITCH DN %.0f", fabs(attitude.pitch));
        }
        lcd_fb_draw_string(warning_x + 5, warning_y + 10, warning_text, COLOR_WHITE, COLOR_RED);
    }

    // Draw bank/roll warning if exceeding ±30 degrees
    if (fabs(attitude.roll) > 30.0f) {
        // Red warning box at top center
        int warning_width = 120;
        int warning_height = 30;
        int warning_x = SCREEN_CENTER_X - warning_width / 2;
        int warning_y = 45;

        // Draw red background box
        lcd_fb_fill_rect(warning_x, warning_y, warning_width, warning_height, COLOR_RED);

        // Draw warning text
        char warning_text[20];
        if (attitude.roll > 30.0f) {
            snprintf(warning_text, sizeof(warning_text), "BANK R %.0f", attitude.roll);
        } else {
            snprintf(warning_text, sizeof(warning_text), "BANK L %.0f", fabs(attitude.roll));
        }
        lcd_fb_draw_string(warning_x + 5, warning_y + 10, warning_text, COLOR_WHITE, COLOR_RED);
    }

    // Update last drawn state
    last_drawn_attitude = attitude;

    // Display framebuffer to screen (single SPI transfer)
    lcd_display_framebuffer();
}


/**
 * Update attitude from ADXL345 sensor - RAW data (no filtering like Python)
 * Returns: true (always update for maximum responsiveness)
 */
bool update_attitude_from_sensor(void)
{
    float x_g, y_g, z_g;

    // Read accelerometer data
    if (adxl345_read_axes(adxl345_fd, &x_g, &y_g, &z_g) < 0) {
        return false;
    }

    // Calculate pitch and roll directly - no filtering
    adxl345_calculate_attitude(x_g, y_g, z_g, &attitude.pitch, &attitude.roll);

    // Always update (like Python version)
    return true;
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
void process_serial_input(void)
{
    static char buffer[BUFFER_SIZE];
    static int buf_pos = 0;
    char c;

    while (read(serial_fd, &c, 1) > 0) {
        if (c == '\n' || c == '\r') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';

                // Parse button press for controls
                const char *button = parse_button_press(buffer);
                if (button) {
                    printf("Button: %s\n", button);

                    // Exit on key4
                    if (strcmp(button, "key4") == 0) {
                        printf("Exit requested\n");
                        running = false;
                    }
                    // Other buttons could be used for menu navigation, etc.
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

    // Display splash screen (disabled - PNG support not compiled)
    // printf("Loading splash screen...\n");
    // if (lcd_display_png("../images/output.png") == 0) {
    //     printf("✓ Splash screen displayed\n");
    //     sleep(2);
    // } else {
    //     printf("⚠ Could not load splash screen, continuing...\n");
    // }

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

    // Initialize ADXL345 accelerometer
    printf("Initializing ADXL345 accelerometer...\n");
    adxl345_fd = adxl345_init();
    if (adxl345_fd < 0) {
        fprintf(stderr, "Failed to initialize ADXL345\n");
        fprintf(stderr, "Make sure ADXL345 is connected to I2C\n");
        lcd_clear(COLOR_BLACK);
        lcd_draw_string(40, 100, "ERROR:", COLOR_RED, COLOR_BLACK);
        lcd_draw_string(20, 120, "ADXL345 not found", COLOR_WHITE, COLOR_BLACK);
        sleep(3);
        if (serial_fd >= 0) close(serial_fd);
        lcd_cleanup();
        return 1;
    }
    printf("✓ ADXL345 initialized\n\n");

    // Initialize GPS module
    printf("Initializing GPS module...\n");
    gps_fd = gps_init();
    if (gps_fd < 0) {
        fprintf(stderr, "Warning: Failed to initialize GPS\n");
        fprintf(stderr, "Continuing without GPS data\n");
        gps_fd = -1;  // Mark as unavailable
    } else {
        printf("✓ GPS initialized\n");
    }
    printf("\n");

    // Display state
    bool attitude_changed = true;
    unsigned long last_sensor_update = 0;
    unsigned long last_gps_update = 0;

    // Main loop
    printf("=== Attitude Indicator Active ===\n");
    printf("Reading attitude from ADXL345 accelerometer\n");
    printf("Press KEY4 on Pico to exit\n\n");

    while (running) {
        // Get current time in milliseconds
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned long current_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        // Update attitude from sensor at regular intervals
        if (current_time - last_sensor_update >= SENSOR_UPDATE_MS) {
            if (update_attitude_from_sensor()) {
                attitude_changed = true;
            }
            last_sensor_update = current_time;
        }

        // Update GPS data at regular intervals (slower than attitude)
        if (gps_fd >= 0 && current_time - last_gps_update >= GPS_UPDATE_MS) {
            if (gps_read_data(gps_fd, &gps_data)) {
                attitude_changed = true;  // Force redraw with new GPS data
            }
            last_gps_update = current_time;
        }

        // Draw attitude indicator IMMEDIATELY when attitude changes (like real avionics)
        if (attitude_changed) {
            draw_attitude_indicator();
            attitude_changed = false;
        }

        // Process input from Pico (non-blocking)
        process_serial_input();

        // Small delay to avoid CPU spinning
        usleep(1000); // 1ms (maximum responsiveness)
    }

    // Cleanup
    printf("\nShutting down...\n");
    lcd_clear(COLOR_BLACK);
    if (serial_fd >= 0) {
        close(serial_fd);
    }
    if (adxl345_fd >= 0) {
        adxl345_close(adxl345_fd);
    }
    if (gps_fd >= 0) {
        gps_cleanup(gps_fd);
    }
    lcd_cleanup();
    printf("✓ Cleanup complete\n");

    return 0;
}
