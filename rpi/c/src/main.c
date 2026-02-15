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
#include "../include/mpu6050.h"
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
#define HORIZON_BAR_WIDTH LCD_WIDTH // Full screen width
#define HORIZON_BAR_HEIGHT 4
#define PITCH_SCALE 2 // pixels per degree
#define AIRCRAFT_SYMBOL_SIZE 40

// Speed/Altitude tape configuration
#define TAPE_WIDTH 15
#define TAPE_HEIGHT LCD_HEIGHT // Full screen height
#define TAPE_MARGIN 5

// Global variables
static volatile bool running = true;
static int serial_fd = -1;
static int mpu6050_fd = -1;
static int gps_fd = -1;

// Attitude data
typedef struct
{
    float pitch; // degrees (positive = nose up)
    float roll;  // degrees (positive = right wing down)
} AttitudeData;

static AttitudeData attitude = {0.0f, 0.0f};
static AttitudeData display_attitude = {0.0f, 0.0f}; // Interpolated attitude for display
static AttitudeData last_drawn_attitude = {999.0f, 999.0f}; // Force initial draw

// Low-pass filter - very light smoothing since MPU6050 has hardware DLPF
// Alpha = 0.95 provides minimal smoothing, maximum responsiveness
#define FILTER_ALPHA 0.95f
static AttitudeData filtered_attitude = {0.0f, 0.0f};
static bool filter_initialized = false;

// Update rates - optimized for smooth display
#define SENSOR_UPDATE_MS 5      // 200 Hz sensor reading
#define DISPLAY_UPDATE_MS 16    // ~60 FPS display refresh (smooth animation)
#define GPS_UPDATE_MS 200       // 5 Hz update rate
#define TELEMETRY_UPDATE_MS 3000  // Send telemetry to Pico every 3 seconds

// Motion interpolation - smoothly interpolate between sensor readings
#define INTERPOLATION_FACTOR 0.3f  // How quickly display catches up to sensor (0.3 = smooth but responsive)

// GPS data
static GPSData gps_data = {0.0f, 0.0f, false, 0};

// Attitude offsets for calibration
static float pitch_offset = 0.0f;
static float roll_offset = 0.0f;

// WiFi status
static bool wifi_connected = false;

/**
 * Check if WiFi is connected by checking if wlan0 has an IP address
 */
bool check_wifi_status(void)
{
    FILE *fp = popen("ip addr show wlan0 | grep 'inet ' | wc -l", "r");
    if (fp == NULL)
    {
        return false;
    }

    char output[16];
    if (fgets(output, sizeof(output), fp) != NULL)
    {
        int count = atoi(output);
        pclose(fp);
        return count > 0; // If wlan0 has an IP address, WiFi is connected
    }

    pclose(fp);
    return false;
}

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
    if (fd < 0)
    {
        perror("Failed to open serial device");
        return -1;
    }

    // Get current options
    if (tcgetattr(fd, &options) < 0)
    {
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

    // Set to non-blocking (no timeout)
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    // Apply options
    if (tcsetattr(fd, TCSANOW, &options) < 0)
    {
        perror("Failed to set serial attributes");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

/**
 * Draw pitch ladder lines - rotated with roll
 * Optimized: precalculate trig values, reduce bounds checking
 */
void draw_pitch_ladder(float pitch, float roll)
{
    // Convert roll to radians and precalculate trig (once per frame)
    float roll_rad = roll * 0.0174532925f;  // PI/180 precomputed
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);

    // Precalculate these for the loop
    int center_x = SCREEN_CENTER_X;
    int center_y = SCREEN_CENTER_Y;

    // Draw pitch lines at 10 degree intervals
    for (int pitch_angle = -30; pitch_angle <= 30; pitch_angle += 10)
    {
        if (pitch_angle == 0)
            continue; // Skip horizon (drawn separately)

        float y_offset_f = (pitch_angle - pitch) * PITCH_SCALE;

        // Line length based on angle
        int line_len = (pitch_angle % 20 == 0) ? 30 : 20;
        uint16_t color = (pitch_angle > 0) ? COLOR_CYAN : COLOR_WHITE;

        // Calculate rotated endpoints (optimized math)
        float line_len_f = (float)line_len;
        int x1 = center_x + (int)(-line_len_f * cos_roll - y_offset_f * sin_roll);
        int y1 = center_y + (int)(-line_len_f * sin_roll + y_offset_f * cos_roll);
        int x2 = center_x + (int)(line_len_f * cos_roll - y_offset_f * sin_roll);
        int y2 = center_y + (int)(line_len_f * sin_roll + y_offset_f * cos_roll);

        // Quick bounds check - just verify it's somewhat on screen
        if ((x1 >= -50 && x1 < LCD_WIDTH + 50 && y1 >= -50 && y1 < LCD_HEIGHT + 50) ||
            (x2 >= -50 && x2 < LCD_WIDTH + 50 && y2 >= -50 && y2 < LCD_HEIGHT + 50))
        {
            lcd_fb_draw_line(x1, y1, x2, y2, color);

            // Draw pitch value for major lines (only when nearly level)
            if (pitch_angle % 20 == 0 && roll > -45.0f && roll < 45.0f)
            {
                char pitch_str[4];  // Reduced buffer size
                int abs_angle = (pitch_angle > 0) ? pitch_angle : -pitch_angle;
                snprintf(pitch_str, sizeof(pitch_str), "%d", abs_angle);

                // Position text near the line endpoints
                int text_x1 = x1 - 15;
                int text_y1 = y1 - 3;
                int text_x2 = x2 + 5;
                int text_y2 = y2 - 3;

                if (text_x1 >= 0 && text_x1 < LCD_WIDTH - 20 && text_y1 >= 0 && text_y1 < LCD_HEIGHT - 10)
                {
                    lcd_fb_draw_string(text_x1, text_y1, pitch_str, color, COLOR_BLACK);
                }
                if (text_x2 >= 0 && text_x2 < LCD_WIDTH - 20 && text_y2 >= 0 && text_y2 < LCD_HEIGHT - 10)
                {
                    lcd_fb_draw_string(text_x2, text_y2, pitch_str, color, COLOR_BLACK);
                }
            }
        }
    }
}

/**
 * Draw the horizon bar (cyan) - rotated based on roll angle
 * Optimized: precalculate values, reduce multiplications
 */
void draw_horizon(float pitch, float roll)
{
    // Calculate horizon position based on pitch
    int horizon_y = SCREEN_CENTER_Y - (int)(pitch * PITCH_SCALE);

    // Convert roll to radians and precalculate trig
    float roll_rad = roll * 0.0174532925f;  // PI/180 precomputed
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);

    // Precalculate values used in loop
    int half_width = HORIZON_BAR_WIDTH / 2;
    int center_x = SCREEN_CENTER_X;
    float half_width_cos = half_width * cos_roll;
    float half_width_sin = half_width * sin_roll;

    // Draw thick cyan rotated bar for horizon (only 4 lines needed)
    for (int i = 0; i < HORIZON_BAR_HEIGHT; i++)
    {
        // Calculate offset from center line
        float offset_y = (float)(i - HORIZON_BAR_HEIGHT / 2);
        float offset_sin = offset_y * sin_roll;
        float offset_cos = offset_y * cos_roll;

        // Calculate rotated endpoints (optimized)
        int x1 = center_x + (int)(-half_width_cos - offset_sin);
        int y1 = horizon_y + (int)(-half_width_sin + offset_cos);
        int x2 = center_x + (int)(half_width_cos - offset_sin);
        int y2 = horizon_y + (int)(half_width_sin + offset_cos);

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
    for (int angle = -60; angle <= 60; angle += 30)
    {
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
    int base_speed = ((int)speed_knots / 5) * 5; // Round to nearest 5

    for (int i = -50; i <= 50; i += 5)
    {
        int mark_speed = base_speed + i;
        if (mark_speed < 0)
            continue;

        float offset = (mark_speed - speed_knots) * 3; // 3 pixels per knot
        int mark_y = SCREEN_CENTER_Y + (int)offset;

        if (mark_y >= 0 && mark_y < LCD_HEIGHT)
        {
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
    int base_alt = ((int)altitude_meters / 10) * 10; // Round to nearest 10

    for (int i = -100; i <= 100; i += 10)
    {
        int mark_alt = base_alt + i;

        float offset = (mark_alt - altitude_meters) * 2; // 2 pixels per meter
        int mark_y = SCREEN_CENTER_Y + (int)offset;

        if (mark_y >= 0 && mark_y < LCD_HEIGHT)
        {
            // Draw tick mark - longer for 20 meter increments
            int tick_len = (mark_alt % 20 == 0) ? 8 : 5;
            lcd_fb_draw_line(x + TAPE_WIDTH - tick_len, mark_y, x + TAPE_WIDTH, mark_y, COLOR_WHITE);
        }
    }

    // No yellow box - just the tape with tick marks
}

/**
 * Draw complete attitude indicator
 * Uses framebuffer for smooth, flicker-free updates with interpolation
 */
void draw_attitude_indicator(void)
{
    // Smooth interpolation: gradually move display toward actual sensor reading
    // This creates fluid motion between sensor updates
    display_attitude.pitch += (attitude.pitch - display_attitude.pitch) * INTERPOLATION_FACTOR;
    display_attitude.roll += (attitude.roll - display_attitude.roll) * INTERPOLATION_FACTOR;

    // Clear framebuffer to black
    lcd_fb_clear(COLOR_BLACK);

    // Draw components using interpolated attitude for smooth motion
    draw_pitch_ladder(display_attitude.pitch, display_attitude.roll);
    draw_horizon(display_attitude.pitch, display_attitude.roll);
    draw_speed_tape(gps_data.speed_knots);
    draw_altitude_tape(gps_data.altitude_meters);
    draw_aircraft_symbol();
    draw_roll_indicator(display_attitude.roll);

    // Draw GPS fix status
    if (gps_data.has_fix)
    {
        lcd_fb_draw_string(SCREEN_CENTER_X - 15, 10, "GPS", COLOR_GREEN, COLOR_BLACK);
    }
    else
    {
        lcd_fb_draw_string(SCREEN_CENTER_X - 20, 10, "NO GPS", COLOR_RED, COLOR_BLACK);
    }

    // Draw pitch warning if exceeding ±20 degrees
    if (fabs(attitude.pitch) > 20.0f)
    {
        // Red warning box at bottom center
        int warning_width = 120;
        int warning_height = 30;
        int warning_x = SCREEN_CENTER_X - warning_width / 2;
        int warning_y = LCD_HEIGHT - warning_height - 10;

        // Draw red background box
        lcd_fb_fill_rect(warning_x, warning_y, warning_width, warning_height, COLOR_RED);

        // Draw warning text
        char warning_text[20];
        if (attitude.pitch > 20.0f)
        {
            snprintf(warning_text, sizeof(warning_text), "PITCH UP %.0f", attitude.pitch);
        }
        else
        {
            snprintf(warning_text, sizeof(warning_text), "PITCH DN %.0f", fabs(attitude.pitch));
        }
        lcd_fb_draw_string(warning_x + 5, warning_y + 10, warning_text, COLOR_WHITE, COLOR_RED);
    }

    // Draw bank/roll warning - threshold depends on speed
    // Low speed (≤85 knots): 20° bank limit (stall prevention)
    // Higher speed (>85 knots): 30° bank limit
    float bank_limit = (gps_data.speed_knots <= 85.0f) ? 20.0f : 30.0f;

    if (fabs(attitude.roll) > bank_limit)
    {
        // Red warning box at top center
        int warning_width = 120;
        int warning_height = 30;
        int warning_x = SCREEN_CENTER_X - warning_width / 2;
        int warning_y = 45;

        // Draw red background box
        lcd_fb_fill_rect(warning_x, warning_y, warning_width, warning_height, COLOR_RED);

        // Draw warning text
        char warning_text[20];
        if (attitude.roll > bank_limit)
        {
            snprintf(warning_text, sizeof(warning_text), "BANK R %.0f", attitude.roll);
        }
        else
        {
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
 * Update attitude from MPU-6050 sensor with low-pass filter for vibration rejection
 * Returns: true if attitude changed significantly
 */
bool update_attitude_from_sensor(void)
{
    float x_g, y_g, z_g;
    float raw_pitch, raw_roll;

    // Read accelerometer data
    if (mpu6050_read_accel(mpu6050_fd, &x_g, &y_g, &z_g) < 0)
    {
        return false;
    }

    // Calculate pitch and roll from raw sensor data
    mpu6050_calculate_attitude(x_g, y_g, z_g, &raw_pitch, &raw_roll);

    // Apply calibration offsets to raw data
    raw_pitch += pitch_offset;
    raw_roll += roll_offset;

    // Initialize filter on first reading
    if (!filter_initialized)
    {
        filtered_attitude.pitch = raw_pitch;
        filtered_attitude.roll = raw_roll;
        filter_initialized = true;
    }
    else
    {
        // Low-pass filter: filtered = alpha * raw + (1 - alpha) * filtered
        // This filters out high-frequency engine vibrations while staying responsive
        filtered_attitude.pitch = FILTER_ALPHA * raw_pitch + (1.0f - FILTER_ALPHA) * filtered_attitude.pitch;
        filtered_attitude.roll = FILTER_ALPHA * raw_roll + (1.0f - FILTER_ALPHA) * filtered_attitude.roll;
    }

    // Update attitude with filtered values
    attitude.pitch = filtered_attitude.pitch;
    attitude.roll = filtered_attitude.roll;

    return true;
}

/**
 * Parse button message from Pico
 * Format: BTN:<button_name>:PRESSED or BTN:<button_name>:RELEASED
 * Returns: button name string, or NULL if not a valid button press
 */
const char *parse_button_press(const char *message)
{
    static char button_name[32];

    // Check if it starts with "BTN:" and ends with ":PRESSED"
    if (strncmp(message, "BTN:", 4) != 0)
    {
        return NULL;
    }

    const char *end = strstr(message, ":PRESSED");
    if (!end)
    {
        return NULL;
    }

    // Extract button name
    int name_len = end - (message + 4);
    if (name_len <= 0 || name_len >= 32)
    {
        return NULL;
    }

    strncpy(button_name, message + 4, name_len);
    button_name[name_len] = '\0';

    return button_name;
}

/**
 * Parse CMD message from Pico (high-level commands)
 * Format: CMD:OFFSET:PITCH:5 or CMD:OFFSET:ROLL:-3
 */
void parse_cmd_message(const char *message)
{
    // Check if it starts with "CMD:"
    if (strncmp(message, "CMD:", 4) != 0)
    {
        return;
    }

    const char *cmd = message + 4;

    // Handle offset commands: OFFSET:PITCH:5 or OFFSET:ROLL:-3
    if (strncmp(cmd, "OFFSET:PITCH:", 13) == 0)
    {
        int offset_value = atoi(cmd + 13);
        pitch_offset = (float)offset_value;
        printf("Pitch offset set to: %.1f degrees\n", pitch_offset);
    }
    else if (strncmp(cmd, "OFFSET:ROLL:", 12) == 0)
    {
        int offset_value = atoi(cmd + 12);
        roll_offset = (float)offset_value;
        printf("Roll offset set to: %.1f degrees\n", roll_offset);
    }
    else if (strcmp(cmd, "OFFSET_MODE") == 0)
    {
        printf("Entering offset adjustment mode\n");
    }
    else if (strcmp(cmd, "OFFSET_EXIT") == 0)
    {
        printf("Exiting offset adjustment mode\n");
        printf("Final offsets - Pitch: %.1f, Roll: %.1f\n", pitch_offset, roll_offset);
    }
}

/**
 * Send telemetry to Pico2 via USB serial
 * Format: JSON with status and warnings
 */
void send_telemetry_to_pico(void)
{
    if (serial_fd < 0)
    {
        return; // No serial connection
    }

    // Calculate warning states
    float bank_limit = (gps_data.speed_knots <= 85.0f) ? 20.0f : 30.0f;
    bool bank_warning = fabs(attitude.roll) > bank_limit;
    bool pitch_warning = fabs(attitude.pitch) > 20.0f;

    // Build JSON telemetry string
    char telemetry[512];
    snprintf(telemetry, sizeof(telemetry),
             "{\"own\":{\"lat\":0.0,\"lon\":0.0,\"alt\":0.0,\"pitch\":%.1f,\"roll\":%.1f,\"yaw\":0.0},"
             "\"traffic\":[],"
             "\"status\":{\"wifi\":%s,\"gps\":%s,\"bluetooth\":false},"
             "\"warnings\":{\"bank\":%s,\"pitch\":%s}}\n",
             attitude.pitch,
             attitude.roll,
             wifi_connected ? "true" : "false",
             gps_data.has_fix ? "true" : "false",
             bank_warning ? "true" : "false",
             pitch_warning ? "true" : "false");

    // Display telemetry in terminal
    printf("\n[TELEMETRY SENT]\n");
    printf("  Attitude: Pitch=%.1f° Roll=%.1f°\n", attitude.pitch, attitude.roll);
    printf("  Status: GPS=%s WiFi=%s\n",
           gps_data.has_fix ? "OK" : "NO_FIX",
           wifi_connected ? "OK" : "OFF");
    printf("  Warnings: BANK=%s (limit=%.0f°) PITCH=%s\n",
           bank_warning ? "ACTIVE" : "off",
           bank_limit,
           pitch_warning ? "ACTIVE" : "off");
    printf("  Speed: %.1f knots\n", gps_data.speed_knots);

    // Send to Pico via serial
    write(serial_fd, telemetry, strlen(telemetry));
}

/**
 * Process serial input from Pico (non-blocking, time-limited)
 */
void process_serial_input(void)
{
    static char buffer[BUFFER_SIZE];
    static int buf_pos = 0;
    char c;
    int chars_read = 0;
    const int MAX_CHARS_PER_CALL = 32; // Limit processing to prevent blocking

    // Read up to MAX_CHARS_PER_CALL characters to prevent blocking for too long
    while (chars_read < MAX_CHARS_PER_CALL && read(serial_fd, &c, 1) > 0)
    {
        chars_read++;

        if (c == '\n' || c == '\r')
        {
            if (buf_pos > 0)
            {
                buffer[buf_pos] = '\0';

                // Parse CMD messages first
                parse_cmd_message(buffer);

                // Parse button press for controls
                const char *button = parse_button_press(buffer);
                if (button)
                {
                    printf("Button: %s\n", button);

                    // Exit on key4
                    if (strcmp(button, "key4") == 0)
                    {
                        printf("Exit requested\n");
                        running = false;
                    }
                    // Other buttons could be used for menu navigation, etc.
                }

                buf_pos = 0;
            }
        }
        else if (buf_pos < BUFFER_SIZE - 1)
        {
            buffer[buf_pos++] = c;
        }
        else
        {
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
    if (lcd_init() < 0)
    {
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

    // Initialize serial connection to Pico (optional - attitude indicator works without it)
    printf("Connecting to Pico...\n");
    serial_fd = serial_init(PICO_DEVICE);
    if (serial_fd < 0)
    {
        fprintf(stderr, "⚠ Failed to open serial connection to Pico\n");
        fprintf(stderr, "⚠ Continuing without Pico (attitude indicator will still work)\n");
        serial_fd = -1; // Mark as unavailable
    }
    else
    {
        printf("✓ Connected to Pico\n");
    }
    printf("\n");

    // Initialize MPU-6050 IMU
    printf("Initializing MPU-6050 IMU...\n");
    mpu6050_fd = mpu6050_init();
    if (mpu6050_fd < 0)
    {
        fprintf(stderr, "Failed to initialize MPU-6050\n");
        fprintf(stderr, "Make sure MPU-6050 is connected to I2C\n");
        lcd_clear(COLOR_BLACK);
        lcd_draw_string(40, 100, "ERROR:", COLOR_RED, COLOR_BLACK);
        lcd_draw_string(20, 120, "MPU-6050 not found", COLOR_WHITE, COLOR_BLACK);
        sleep(3);
        if (serial_fd >= 0)
            close(serial_fd);
        lcd_cleanup();
        return 1;
    }
    printf("✓ MPU-6050 initialized\n\n");

    // Initialize GPS module
    printf("Initializing GPS module...\n");
    gps_fd = gps_init();
    if (gps_fd < 0)
    {
        fprintf(stderr, "Warning: Failed to initialize GPS\n");
        fprintf(stderr, "Continuing without GPS data\n");
        gps_fd = -1; // Mark as unavailable
    }
    else
    {
        printf("✓ GPS initialized\n");
    }

    // Check WiFi status
    printf("Checking WiFi status...\n");
    wifi_connected = check_wifi_status();
    if (wifi_connected)
    {
        printf("✓ WiFi connected\n");
    }
    else
    {
        printf("⚠ WiFi not connected\n");
    }
    printf("\n");

    // Display state
    unsigned long last_sensor_update = 0;
    unsigned long last_display_update = 0;
    unsigned long last_gps_update = 0;
    unsigned long last_telemetry_update = 0;
    unsigned long last_wifi_check = 0;

    // Main loop
    printf("=== Attitude Indicator Active ===\n");
    printf("Reading attitude from MPU-6050 IMU\n");
    printf("Display updating at 60 FPS for smooth motion\n");
    if (serial_fd >= 0)
    {
        printf("Press KEY4 on Pico to exit\n");
    }
    else
    {
        printf("Press Ctrl+C to exit (Pico not connected)\n");
    }
    printf("\n");

    while (running)
    {
        // Get current time in milliseconds
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned long current_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        // Update attitude from sensor at regular intervals
        if (current_time - last_sensor_update >= SENSOR_UPDATE_MS)
        {
            update_attitude_from_sensor();
            last_sensor_update = current_time;
        }

        // Update GPS data at regular intervals (slower than attitude)
        if (gps_fd >= 0 && current_time - last_gps_update >= GPS_UPDATE_MS)
        {
            gps_read_data(gps_fd, &gps_data);
            last_gps_update = current_time;
        }

        // Refresh display at fixed 60 FPS for smooth animation
        // This ensures we always see intermediate frames during fast movements
        if (current_time - last_display_update >= DISPLAY_UPDATE_MS)
        {
            draw_attitude_indicator();
            last_display_update = current_time;
        }

        // Send telemetry to Pico at regular intervals (only if connected)
        if (serial_fd >= 0 && current_time - last_telemetry_update >= TELEMETRY_UPDATE_MS)
        {
            send_telemetry_to_pico();
            last_telemetry_update = current_time;
        }

        // Check WiFi status every 30 seconds
        if (current_time - last_wifi_check >= 30000)
        {
            wifi_connected = check_wifi_status();
            last_wifi_check = current_time;
        }

        // Process input from Pico (non-blocking, only if connected)
        if (serial_fd >= 0)
        {
            process_serial_input();
        }

        // No delay - run at maximum speed, sensor polling controls update rate
        // This ensures immediate response to attitude changes
    }

    // Cleanup
    printf("\nShutting down...\n");
    lcd_clear(COLOR_BLACK);
    if (serial_fd >= 0)
    {
        close(serial_fd);
    }
    if (mpu6050_fd >= 0)
    {
        mpu6050_close(mpu6050_fd);
    }
    if (gps_fd >= 0)
    {
        gps_cleanup(gps_fd);
    }
    lcd_cleanup();
    printf("✓ Cleanup complete\n");

    return 0;
}
