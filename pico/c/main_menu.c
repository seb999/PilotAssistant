/**
 * Pico2 Menu System
 * Interactive menu with joystick navigation
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "st7789_lcd.h"
#include "img/splash_data.h"
#include "input_handler.h"
#include "menu.h"
#include "telemetry_parser.h"
#include "icm20948_sensor.h"
#include "madgwick_filter.h"

#define LED_PIN 25
#define BUFFER_SIZE 1024

// Telemetry data and state (used by RADAR)
static TelemetryData latest_telemetry;
static bool telemetry_received = false;
static char rx_buffer[BUFFER_SIZE];
static size_t rx_index = 0;

// Track button release events for USB command sending
typedef struct {
    bool key1_was_pressed;
    bool key2_was_pressed;
    bool key4_was_pressed;
    bool press_was_pressed;
    bool up_was_pressed;
    bool down_was_pressed;
    bool left_was_pressed;
    bool right_was_pressed;
} ButtonReleaseTracker;

static ButtonReleaseTracker release_tracker = {0};

// Status ribbon state tracking (to avoid unnecessary redraws)
typedef struct {
    bool telemetry_received;
    bool wifi_ok;
    bool gps_ok;
    bool bt_ok;
    bool any_warning;
} RibbonState;

static RibbonState last_ribbon_state = {false, false, false, false, false};

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

// Internal function to actually draw the ribbon (no state check)
static void draw_ribbon_internal(void) {
    // Build current state
    RibbonState current_state;
    current_state.telemetry_received = telemetry_received;
    current_state.any_warning = false;

    if (telemetry_received) {
        current_state.wifi_ok = latest_telemetry.status.wifi;
        current_state.gps_ok = latest_telemetry.status.gps;
        current_state.bt_ok = latest_telemetry.status.bluetooth;
        current_state.any_warning = latest_telemetry.warnings.bank_warning ||
                                    latest_telemetry.warnings.pitch_warning;
    } else {
        current_state.wifi_ok = false;
        current_state.gps_ok = false;
        current_state.bt_ok = false;
    }

    // Update state
    last_ribbon_state = current_state;

    // Draw ribbon background - RED if warning active, dark gray otherwise
    uint16_t ribbon_color = current_state.any_warning ? COLOR_RED : 0x2104;
    lcd_fill_rect(0, 0, 320, 28, ribbon_color);

    // If warning active, display WARNING text on left side
    if (current_state.any_warning) {
        lcd_draw_string_scaled(5, 6, "WARNING", COLOR_WHITE, COLOR_RED, 2);
    }

    // Draw status icons on right side
    lcd_draw_wifi_icon(240, 2, current_state.wifi_ok);
    lcd_draw_gps_icon(268, 2, current_state.gps_ok);
    lcd_draw_bluetooth_icon(296, 2, current_state.bt_ok);
    lcd_flush_rect(0, 0, 320, 28);
}

// Force draw ribbon (always draw, ignore state check)
// Used when returning to menu to ensure ribbon is immediately visible
void draw_ribbon_force(void) {
    draw_ribbon_internal();
}

// Function to draw status ribbon with icons and warnings at the top
// Only redraws if state has changed (no blinking)
void draw_status_icons(void) {
    // Build current state
    RibbonState current_state;
    current_state.telemetry_received = telemetry_received;
    current_state.any_warning = false;

    if (telemetry_received) {
        current_state.wifi_ok = latest_telemetry.status.wifi;
        current_state.gps_ok = latest_telemetry.status.gps;
        current_state.bt_ok = latest_telemetry.status.bluetooth;
        current_state.any_warning = latest_telemetry.warnings.bank_warning ||
                                    latest_telemetry.warnings.pitch_warning;
    } else {
        current_state.wifi_ok = false;
        current_state.gps_ok = false;
        current_state.bt_ok = false;
    }

    // Check if anything changed
    bool changed = (current_state.telemetry_received != last_ribbon_state.telemetry_received) ||
                   (current_state.wifi_ok != last_ribbon_state.wifi_ok) ||
                   (current_state.gps_ok != last_ribbon_state.gps_ok) ||
                   (current_state.bt_ok != last_ribbon_state.bt_ok) ||
                   (current_state.any_warning != last_ribbon_state.any_warning);

    // Only redraw if something changed
    if (!changed) {
        return;
    }

    // Draw the ribbon
    draw_ribbon_internal();
}

// Read and process serial telemetry data
void read_telemetry_data() {
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        char ch = (char)c;
        if (ch == '\n' || ch == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                if (parse_telemetry(rx_buffer, &latest_telemetry)) {
                    telemetry_received = true;  // Mark that we've received telemetry
                }
                rx_index = 0;
            }
        } else if (rx_index < BUFFER_SIZE - 1) {
            rx_buffer[rx_index++] = ch;
        } else {
            rx_index = 0;
        }
    }
}

// Menu action functions
void action_go_fly(void) {
    printf("=== GO FLY selected ===\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(80, 100, "GO FLY", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    sleep_ms(2000);
}

void action_bluetooth(void) {
    printf("=== BLUETOOTH selected ===\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(70, 100, "BLUETOOTH", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    sleep_ms(2000);
}

void action_gyro_offset(void) {
    printf("=== GYRO OFFSET selected ===\n");

    // Current offsets (persistent across function calls)
    static int pitch_offset = 0;
    static int roll_offset = 0;

    // Send command to enter offset mode
    send_high_level_command("OFFSET_MODE");

    InputState input_state = {0};
    char buf[64];

    // Initial full screen draw
    lcd_clear(COLOR_BLACK);

    // Title
    lcd_draw_string_scaled(40, 10, "GYRO OFFSET", COLOR_CYAN, COLOR_BLACK, 3);

    // Instructions
    lcd_draw_string(20, 60, "UP/DOWN: Pitch offset", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 75, "LEFT/RIGHT: Roll offset", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 90, "KEY2: Back to menu", COLOR_WHITE, COLOR_BLACK);

    // Display initial offsets (large text)
    snprintf(buf, sizeof(buf), "PITCH: %+d", pitch_offset);
    lcd_draw_string_scaled(50, 120, buf, COLOR_YELLOW, COLOR_BLACK, 3);

    snprintf(buf, sizeof(buf), "ROLL:  %+d", roll_offset);
    lcd_draw_string_scaled(50, 155, buf, COLOR_YELLOW, COLOR_BLACK, 3);
    lcd_flush();

    // Offset adjustment loop
    while (true) {
        input_read(&input_state);

        // Adjust pitch offset with UP/DOWN - only update the value area
        if (input_just_pressed_up(&input_state)) {
            pitch_offset++;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "OFFSET:PITCH:%d", pitch_offset);
            send_high_level_command(cmd);

            // Clear only the pitch value area (black rectangle)
            lcd_fill_rect(50, 120, 220, 24, COLOR_BLACK);

            // Redraw only the pitch value
            snprintf(buf, sizeof(buf), "PITCH: %+d", pitch_offset);
            lcd_draw_string_scaled(50, 120, buf, COLOR_YELLOW, COLOR_BLACK, 3);
            lcd_flush_rect(50, 120, 220, 24);

            printf("Pitch offset: %+d\n", pitch_offset);
        }

        if (input_just_pressed_down(&input_state)) {
            pitch_offset--;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "OFFSET:PITCH:%d", pitch_offset);
            send_high_level_command(cmd);

            lcd_fill_rect(50, 120, 220, 24, COLOR_BLACK);
            snprintf(buf, sizeof(buf), "PITCH: %+d", pitch_offset);
            lcd_draw_string_scaled(50, 120, buf, COLOR_YELLOW, COLOR_BLACK, 3);
            lcd_flush_rect(50, 120, 220, 24);

            printf("Pitch offset: %+d\n", pitch_offset);
        }

        // Adjust roll offset with LEFT/RIGHT
        if (input_just_pressed_left(&input_state)) {
            roll_offset--;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "OFFSET:ROLL:%d", roll_offset);
            send_high_level_command(cmd);

            lcd_fill_rect(50, 155, 220, 24, COLOR_BLACK);
            snprintf(buf, sizeof(buf), "ROLL:  %+d", roll_offset);
            lcd_draw_string_scaled(50, 155, buf, COLOR_YELLOW, COLOR_BLACK, 3);
            lcd_flush_rect(50, 155, 220, 24);

            printf("Roll offset: %+d\n", roll_offset);
        }

        if (input_just_pressed_right(&input_state)) {
            roll_offset++;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "OFFSET:ROLL:%d", roll_offset);
            send_high_level_command(cmd);

            lcd_fill_rect(50, 155, 220, 24, COLOR_BLACK);
            snprintf(buf, sizeof(buf), "ROLL:  %+d", roll_offset);
            lcd_draw_string_scaled(50, 155, buf, COLOR_YELLOW, COLOR_BLACK, 3);
            lcd_flush_rect(50, 155, 220, 24);

            printf("Roll offset: %+d\n", roll_offset);
        }

        // Exit on KEY2 (back to menu)
        if (input_just_pressed_key2(&input_state)) {
            printf("Returning to menu (KEY2 pressed)\n");
            send_high_level_command("OFFSET_EXIT");
            break;
        }

        sleep_ms(50);
    }
}

// OLD TELEMETRY MENU REMOVED - Use RADAR instead

// Helper function to convert lat/lon distance to pixels on radar
void latlon_to_radar_xy(double own_lat, double own_lon, double ac_lat, double ac_lon,
                        int16_t* out_x, int16_t* out_y, uint16_t radar_center_x, uint16_t radar_center_y) {
    // Earth radius in km
    const double R = 6371.0;

    // Convert to radians
    double lat1 = own_lat * 3.14159265359 / 180.0;
    double lon1 = own_lon * 3.14159265359 / 180.0;
    double lat2 = ac_lat * 3.14159265359 / 180.0;
    double lon2 = ac_lon * 3.14159265359 / 180.0;

    // Calculate x (east-west) and y (north-south) distances in km
    double dx_km = (lon2 - lon1) * cos((lat1 + lat2) / 2.0) * R;
    double dy_km = (lat2 - lat1) * R;

    // Convert km to pixels (scale: 25km = 80 pixels, so 1km = 3.2 pixels)
    const double km_to_pixels = 3.2;
    *out_x = radar_center_x + (int16_t)(dx_km * km_to_pixels);
    *out_y = radar_center_y - (int16_t)(dy_km * km_to_pixels);  // Subtract because screen Y increases downward
}

// Draw static radar elements (called once)
void draw_radar_static(void) {
    const uint16_t radar_center_x = 160;
    const uint16_t radar_center_y = 120;

    // Draw 3 concentric circles with equal spacing (35 pixels each) - use full screen height
    lcd_draw_circle(radar_center_x, radar_center_y, 35, COLOR_WHITE);   // Inner ring
    lcd_draw_circle(radar_center_x, radar_center_y, 70, COLOR_WHITE);   // Middle ring
    lcd_draw_circle(radar_center_x, radar_center_y, 105, COLOR_WHITE);  // Outer ring

    // Draw ownship at center (yellow square)
    lcd_fill_rect(radar_center_x - 2, radar_center_y - 2, 5, 5, COLOR_YELLOW);

    // Draw status icons at top right
    draw_status_icons();
    lcd_flush();
}

// Clear previous aircraft positions
static int16_t prev_aircraft_x[10];
static int16_t prev_aircraft_y[10];
static uint8_t prev_aircraft_count = 0;

// Update radar aircraft positions (called repeatedly)
void update_radar_aircraft(void) {
    char buf[32];
    const uint16_t radar_center_x = 160;
    const uint16_t radar_center_y = 120;

    // Clear previous aircraft positions
    for (int i = 0; i < prev_aircraft_count; i++) {
        if (prev_aircraft_x[i] >= 0 && prev_aircraft_y[i] >= 0) {
            lcd_fill_rect(prev_aircraft_x[i] - 3, prev_aircraft_y[i] - 5, 50, 10, COLOR_BLACK);
        }
    }

    // Draw status icons
    draw_status_icons();

    // Draw new aircraft positions
    prev_aircraft_count = 0;
    for (int i = 0; i < latest_telemetry.traffic_count && i < 10; i++) {
        TrafficData* t = &latest_telemetry.traffic[i];
        int16_t screen_x, screen_y;

        latlon_to_radar_xy(latest_telemetry.own.lat, latest_telemetry.own.lon,
                          t->lat, t->lon, &screen_x, &screen_y,
                          radar_center_x, radar_center_y);

        // Draw aircraft if within screen bounds (expanded for debugging)
        if (screen_x >= 0 && screen_x <= 320 && screen_y >= 0 && screen_y <= 240) {
            // Draw aircraft as red square
            lcd_fill_rect(screen_x - 2, screen_y - 2, 4, 4, COLOR_RED);

            // Draw callsign nearby (only for first 3 to avoid clutter)
            if (i < 3) {
                lcd_draw_string(screen_x + 4, screen_y - 4, t->id, COLOR_RED, COLOR_BLACK);
            }

            // Store position for next clear
            prev_aircraft_x[prev_aircraft_count] = screen_x;
            prev_aircraft_y[prev_aircraft_count] = screen_y;
            prev_aircraft_count++;
        }
    }

    // Draw traffic count at bottom
    snprintf(buf, sizeof(buf), "TRAFFIC: %d", latest_telemetry.traffic_count);
    lcd_fill_rect(5, 220, 150, 10, COLOR_BLACK);  // Clear old count
    lcd_draw_string(5, 220, buf, COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
}

void action_radar(void) {
    printf("=== RADAR selected ===\n");

    telemetry_received = false;

    // Reset previous aircraft tracking
    prev_aircraft_count = 0;

    // Clear screen and draw radar immediately
    lcd_clear(COLOR_BLACK);
    draw_radar_static();

    InputState input_state = {0};

    // Radar display loop
    while (true) {
        input_read(&input_state);

        // Exit on LEFT button press
        if (input_just_pressed_left(&input_state)) {
            printf("Exiting radar display\n");
            break;
        }

        // Read telemetry data
        read_telemetry_data();

        // Update aircraft positions if we have telemetry
        if (telemetry_received) {
            update_radar_aircraft();
        }

        sleep_ms(500);  // Update every 500ms
    }
}

void action_test_gyro(void) {
    printf("=== AHRS ATTITUDE INDICATOR selected ===\n");

    // Initialize ICM20948
    if (!icm20948_init()) {
        lcd_clear(COLOR_BLACK);
        lcd_draw_string_scaled(20, 60, "SENSOR ERROR", COLOR_RED, COLOR_BLACK, 3);
        lcd_draw_string(20, 110, "WHO_AM_I check failed", COLOR_WHITE, COLOR_BLACK);
        lcd_draw_string(20, 125, "Expected: 0xEA", COLOR_CYAN, COLOR_BLACK);
        lcd_draw_string(20, 145, "Check connections:", COLOR_WHITE, COLOR_BLACK);
        lcd_draw_string(20, 160, "CS:GPIO17 SCK:GPIO18", COLOR_CYAN, COLOR_BLACK);
        lcd_draw_string(20, 175, "MOSI:GPIO19 MISO:GPIO20", COLOR_CYAN, COLOR_BLACK);
        lcd_draw_string(20, 200, "KEY2: Back", COLOR_WHITE, COLOR_BLACK);
        lcd_flush();

        InputState input_state = {0};
        while (true) {
            input_read(&input_state);
            if (input_just_pressed_key2(&input_state)) break;
            sleep_ms(50);
        }
        return;
    }

    // Madgwick IMU-only filter (6DOF)
    MadgwickFilter filter;
    madgwick_init(&filter, 100.0f, 0.10f);

    InputState input_state = {0};
    SensorData accel, gyro;

    // Sensor bias calibration - measure bias while stationary
    printf("Calibrating sensors (keep sensor still)...\n");
    float gyro_bias_x = 0, gyro_bias_y = 0, gyro_bias_z = 0;
    float accel_bias_x = 0, accel_bias_y = 0, accel_bias_z = 0;
    int calibration_samples = 100;

    for (int i = 0; i < calibration_samples; i++) {
        if (icm20948_read_gyro(&gyro)) {
            gyro_bias_x += icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
            gyro_bias_y += icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
            gyro_bias_z += icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);
        }
        if (icm20948_read_accel(&accel)) {
            accel_bias_x += icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G);
            accel_bias_y += icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G);
            accel_bias_z += icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G);
        }
        sleep_ms(10);
    }

    gyro_bias_x /= calibration_samples;
    gyro_bias_y /= calibration_samples;
    gyro_bias_z /= calibration_samples;
    accel_bias_x /= calibration_samples;
    accel_bias_y /= calibration_samples;
    accel_bias_z /= calibration_samples;
    accel_bias_z -= 1.0f;  // Remove gravity from Z

    printf("Gyro bias: X=%+.2f Y=%+.2f Z=%+.2f deg/s\n",
           gyro_bias_x, gyro_bias_y, gyro_bias_z);
    printf("Accel bias: X=%+.4f Y=%+.4f Z=%+.4f g\n",
           accel_bias_x, accel_bias_y, accel_bias_z);

    // Attitude indicator parameters - full screen 320x240
    const int16_t center_x = 160;
    const int16_t center_y = 120;
    absolute_time_t last_update = get_absolute_time();

    // AHRS loop - draw to framebuffer, flush once per frame
    while (true) {
        input_read(&input_state);

        if (input_just_pressed_key2(&input_state)) {
            printf("Returning to menu (KEY2 pressed)\n");
            icm20948_sleep();
            break;
        }

        // Read sensor data
        if (!icm20948_read_accel(&accel) || !icm20948_read_gyro(&gyro))
            continue;

        absolute_time_t now = get_absolute_time();
        int64_t dt_us = absolute_time_diff_us(last_update, now);
        last_update = now;
        float dt = (float)dt_us / 1000000.0f;
        if (dt <= 0.0f || dt > 0.2f) {
            dt = 0.01f;
        }

        // Convert to physical units and subtract bias
        float ax = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G) - accel_bias_x;
        float ay = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G) - accel_bias_y;
        float az = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G) - accel_bias_z;

        float gx_raw_dps = icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
        float gy_raw_dps = icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
        float gz_raw_dps = icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);

        float gx_dps = gx_raw_dps - gyro_bias_x;
        float gy_dps = gy_raw_dps - gyro_bias_y;
        float gz_dps = gz_raw_dps - gyro_bias_z;
        if (!isfinite(gx_dps) || !isfinite(gy_dps) || !isfinite(gz_dps)) {
            continue;
        }
        if (fabsf(gx_dps) > 2000.0f || fabsf(gy_dps) > 2000.0f || fabsf(gz_dps) > 2000.0f) {
            continue;
        }

        // Auto-trim gyro bias while clearly stationary to suppress long-term drift
        float acc_norm = sqrtf(ax * ax + ay * ay + az * az);
        bool stationary = isfinite(acc_norm) &&
                          fabsf(acc_norm - 1.0f) < 0.08f &&
                          fabsf(gx_dps) < 1.2f &&
                          fabsf(gy_dps) < 1.2f &&
                          fabsf(gz_dps) < 1.2f;
        if (stationary) {
            float bias_alpha = dt * 0.08f;  // ~12.5s time constant
            if (bias_alpha > 0.02f) bias_alpha = 0.02f;
            gyro_bias_x = (1.0f - bias_alpha) * gyro_bias_x + bias_alpha * gx_raw_dps;
            gyro_bias_y = (1.0f - bias_alpha) * gyro_bias_y + bias_alpha * gy_raw_dps;
            gyro_bias_z = (1.0f - bias_alpha) * gyro_bias_z + bias_alpha * gz_raw_dps;

            gx_dps = gx_raw_dps - gyro_bias_x;
            gy_dps = gy_raw_dps - gyro_bias_y;
            gz_dps = gz_raw_dps - gyro_bias_z;
        }

        if (fabsf(gz_dps) < 0.15f) {
            gz_dps = 0.0f;
        }
        float gx = gx_dps * M_PI / 180.0f;
        float gy = gy_dps * M_PI / 180.0f;
        float gz = gz_dps * M_PI / 180.0f;

        // Update Madgwick with measured dt (same structure as your reference code)
        filter.sample_freq = 1.0f / dt;
        filter.inv_sample_freq = dt;
        madgwick_update_imu(&filter, gx, gy, gz, ax, ay, az);

        // Map filter angles to current display convention
        float roll = -madgwick_get_roll_deg(&filter);
        float pitch = madgwick_get_pitch_deg(&filter);
        if (!isfinite(roll) || !isfinite(pitch)) {
            madgwick_init(&filter, 100.0f, 0.10f);
            continue;
        }

        if (pitch > 60.0f) pitch = 60.0f;
        if (pitch < -60.0f) pitch = -60.0f;

        float roll_rad = roll * M_PI / 180.0f;
        float cos_r = cosf(roll_rad);
        float sin_r = sinf(roll_rad);
        float pitch_px = pitch * 2.0f;  // 2 pixels per degree

        // ============================================================
        // Draw everything to framebuffer (no SPI until flush)
        // ============================================================

        // Sky/ground with per-pixel scanline rendering
        for (int16_t y = 0; y < LCD_HEIGHT; y++) {
            float dy = (float)(y - center_y) + pitch_px;

            if (fabsf(sin_r) < 0.01f) {
                // Nearly level: entire row is one color
                uint16_t color = (dy < 0) ? COLOR_SKY : COLOR_BROWN;
                lcd_fill_rect(0, y, LCD_WIDTH, 1, color);
            } else {
                // Banked: find where horizon crosses this row
                int16_t x_cross = center_x - (int16_t)(dy * cos_r / sin_r);

                if (x_cross <= 0) {
                    uint16_t color = (sin_r > 0) ? ((dy < 0) ? COLOR_SKY : COLOR_BROWN) : ((dy < 0) ? COLOR_SKY : COLOR_BROWN);
                    // All pixels on one side of horizon
                    float test_dy = dy;
                    float test = cos_r * test_dy + sin_r * (0 - center_x);
                    color = (test < 0) ? COLOR_SKY : COLOR_BROWN;
                    lcd_fill_rect(0, y, LCD_WIDTH, 1, color);
                } else if (x_cross >= LCD_WIDTH) {
                    float test = cos_r * dy + sin_r * (0 - center_x);
                    uint16_t color = (test < 0) ? COLOR_SKY : COLOR_BROWN;
                    lcd_fill_rect(0, y, LCD_WIDTH, 1, color);
                } else {
                    // Horizon crosses this row - determine which side is sky
                    // Test left edge: is it above or below horizon?
                    float test_left = cos_r * dy + sin_r * (0 - center_x);
                    uint16_t left_color = (test_left < 0) ? COLOR_SKY : COLOR_BROWN;
                    uint16_t right_color = (test_left < 0) ? COLOR_BROWN : COLOR_SKY;

                    lcd_fill_rect(0, y, x_cross, 1, left_color);
                    lcd_fill_rect(x_cross, y, LCD_WIDTH - x_cross, 1, right_color);
                }
            }
        }

        // Aircraft symbol (fixed yellow wings at center)
        lcd_draw_line(center_x - 35, center_y, center_x - 8, center_y, COLOR_YELLOW);
        lcd_draw_line(center_x + 8, center_y, center_x + 35, center_y, COLOR_YELLOW);
        lcd_draw_line(center_x - 35, center_y + 1, center_x - 8, center_y + 1, COLOR_YELLOW);
        lcd_draw_line(center_x + 8, center_y + 1, center_x + 35, center_y + 1, COLOR_YELLOW);
        lcd_fill_rect(center_x - 3, center_y - 3, 7, 7, COLOR_YELLOW);

        // ============================================================
        // Single flush - send entire framebuffer to LCD via DMA
        // ============================================================
        lcd_flush();
    }
}

// Menu items (UPPERCASE for font compatibility)
static MenuItem menu_items[] = {
    {"GO FLY", action_go_fly},
    {"BLUETOOTH", action_bluetooth},
    {"GYRO OFFSET", action_gyro_offset},
    {"RADAR", action_radar},
    {"TEST GYRO", action_test_gyro}
};

int main() {
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // Initialize USB serial (stdio)
    stdio_init_all();

    // Wait for USB connection
    sleep_ms(2000);

    printf("\n\n");
    printf("=====================================\n");
    printf("  Pico2 Menu System v1.0\n");
    printf("  With Joystick Navigation\n");
    printf("=====================================\n");

    // Initialize LCD
    printf("Initializing LCD...\n");
    lcd_init();

    // Display splash screen for 2 seconds
    printf("Displaying splash screen...\n");
    lcd_display_splash(splash_320x240_bin, splash_320x240_bin_len);

    // Flash LED during splash
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }
    sleep_ms(1400);  // Total 2 seconds for splash

    // Initialize input handler
    printf("Initializing input handler...\n");
    input_init();
    printf("Input handler ready\n");

    // Initialize menu
    printf("Initializing menu...\n");
    MenuState menu;
    menu_init(&menu, menu_items, 5, NULL);  // 5 items: GO FLY, BLUETOOTH, GYRO OFFSET, RADAR, TEST GYRO

    // Draw initial menu
    menu_draw_full(&menu);
    printf("Menu displayed\n\n");

    // Input state
    InputState input_state = {0};

    // Main loop
    while (true) {
        // Read inputs
        input_read(&input_state);

        // --- Send Button Press Commands to Raspberry Pi ---
        if (input_just_pressed_key1(&input_state)) {
            send_button_command(1, "PRESS");
            send_high_level_command("FLY_MODE");
            release_tracker.key1_was_pressed = true;
        }

        if (input_just_pressed_key2(&input_state)) {
            send_button_command(2, "PRESS");
            send_high_level_command("GYRO_CALIBRATION");
            release_tracker.key2_was_pressed = true;
        }

        if (input_just_pressed_key4(&input_state)) {
            send_button_command(4, "PRESS");
            send_high_level_command("BLUETOOTH");
            release_tracker.key4_was_pressed = true;
        }

        if (input_just_pressed_press(&input_state)) {
            send_button_command(5, "PRESS");
            release_tracker.press_was_pressed = true;
        }

        // --- Send Button Release Commands ---
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

        // --- Send Joystick Commands ---
        if (input_just_pressed_up(&input_state)) {
            send_joystick_command("UP");
            release_tracker.up_was_pressed = true;
        }

        if (input_just_pressed_down(&input_state)) {
            send_joystick_command("DOWN");
            release_tracker.down_was_pressed = true;
        }

        if (input_just_pressed_left(&input_state)) {
            send_joystick_command("LEFT");
            release_tracker.left_was_pressed = true;
        }

        if (input_just_pressed_right(&input_state)) {
            send_joystick_command("RIGHT");
            release_tracker.right_was_pressed = true;
        }

        // --- Joystick Release (optional, for future use) ---
        if (release_tracker.up_was_pressed && !input_state.up) {
            release_tracker.up_was_pressed = false;
        }

        if (release_tracker.down_was_pressed && !input_state.down) {
            release_tracker.down_was_pressed = false;
        }

        if (release_tracker.left_was_pressed && !input_state.left) {
            release_tracker.left_was_pressed = false;
        }

        if (release_tracker.right_was_pressed && !input_state.right) {
            release_tracker.right_was_pressed = false;
        }

        // Read telemetry data from Raspberry Pi
        read_telemetry_data();

        // Handle menu navigation
        if (menu_handle_input(&menu, &input_state)) {
            // An action was selected and executed
            // Redraw the menu after action completes
            menu_draw_full(&menu);
        }

        // Update status ribbon periodically (every 100ms)
        static uint32_t last_ribbon_update = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_ribbon_update >= 100) {
            draw_status_icons();
            last_ribbon_update = now;
        }

        // Small delay
        sleep_ms(10);
    }

    return 0;
}
