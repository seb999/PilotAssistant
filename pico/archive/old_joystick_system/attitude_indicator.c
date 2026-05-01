/**
 * Attitude Indicator for Raspberry Pi Pico 2
 * Hardware: ICM-20948 (9DOF IMU) + ST7789 LCD (320x240)
 * Based on ESP32 attitude indicator with Madgwick AHRS filter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "icm20948_sensor.h"
#include "st7789_lcd.h"
#include "madgwick_filter.h"
#include "attitude_indicator.h"
#include "input_handler.h"
#include "telemetry_parser.h"

// External telemetry data from main_menu.c
extern TelemetryData latest_telemetry;
extern bool telemetry_received;
extern void read_telemetry_data(void);

// ===================== USER SETTINGS =====================
// Set to 1 for heading using magnetometer (requires mag calibration for best HDG)
// Set to 0 for best roll/pitch stability (yaw/HDG will drift)
#define USE_MAG 1

// Filter update rate (Hz)
static const uint32_t LOOP_HZ = 100;
static const uint32_t LOOP_US = 1000000UL / LOOP_HZ;

// How long to calibrate gyro at boot (ms)
static const uint32_t GYRO_CAL_MS = 1500;  // Reduced from 2500ms (sufficient with DLPF)

// UI smoothing (extra) - optimized for maximum responsiveness
static const float UI_SMOOTH = 0.12f;  // lower = snappier (was 0.15f)

// Pitch visual scaling: pixels per degree
static const float PITCH_PIX_PER_DEG = 2.5f;

// Limit display angles
static const float MAX_DISPLAY_DEG = 80.0f;

// ===================== IMU & FILTER =====================
static MadgwickFilter filter;
static GyroRange gyro_range = GYRO_RANGE_500DPS;
static AccelRange accel_range = ACCEL_RANGE_4G;

// Gyro bias (deg/s)
static float gx_bias = 0.0f;
static float gy_bias = 0.0f;
static float gz_bias = 0.0f;

// Filter state (smoothed for UI)
static float smooth_roll = 0.0f;
static float smooth_pitch = 0.0f;

// ===================== UTILITY FUNCTIONS =====================
static float wrap360(float deg) {
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// ===================== STATUS DISPLAY =====================
static void lcd_status(const char* line1, const char* line2) {
    lcd_clear(COLOR_BLACK);

    // Draw centered text (scale 2)
    // "Initialisation" = 14 chars * 12 pixels = 168 pixels wide
    // Center: (320 - 168) / 2 = 76
    lcd_draw_string_scaled(76, 110, line1, COLOR_WHITE, COLOR_BLACK, 2);

    if (line2 && strlen(line2) > 0) {
        lcd_draw_string_scaled(10, 140, line2, COLOR_WHITE, COLOR_BLACK, 2);
    }
    lcd_flush();
}

// ===================== GYRO CALIBRATION =====================
static void calibrate_gyro_bias(void) {
    printf("Calibrating gyro bias: keep STILL...\n");

    double sx = 0.0, sy = 0.0, sz = 0.0;
    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    int n = 0;

    SensorData gyro;
    while (to_ms_since_boot(get_absolute_time()) - start_ms < GYRO_CAL_MS) {
        if (icm20948_read_gyro(&gyro)) {
            float gx = icm20948_gyro_to_dps(gyro.x, gyro_range);
            float gy = icm20948_gyro_to_dps(gyro.y, gyro_range);
            float gz = icm20948_gyro_to_dps(gyro.z, gyro_range);

            sx += gx;
            sy += gy;
            sz += gz;
            n++;
        }
        sleep_ms(5);
    }

    if (n > 0) {
        gx_bias = (float)(sx / n);
        gy_bias = (float)(sy / n);
        gz_bias = (float)(sz / n);
    }

    printf("Gyro bias (dps): %.4f, %.4f, %.4f\n", gx_bias, gy_bias, gz_bias);
}

// ===================== SETUP =====================
static void setup(void) {
    printf("\n=== Attitude Indicator Startup ===\n");

    // Show single initialization message to guide user
    lcd_status("GYRO INITIALIZING", "HOLD DEVICE STEADY");

    // Initialize ICM-20948
    if (!icm20948_init()) {
        lcd_status("ICM-20948 FAILED", "Check wiring!");
        printf("ERROR: ICM-20948 init failed\n");
        while (1) {
            sleep_ms(1000);
        }
    }

#if USE_MAG
    // Initialize magnetometer
    if (!icm20948_init_magnetometer()) {
        printf("WARNING: Magnetometer init failed, using IMU-only mode\n");
    }
#endif

    // Initialize Madgwick filter (beta optimized for fast convergence)
    madgwick_init(&filter, (float)LOOP_HZ, 0.15f);  // Lower beta = trust gyro more, faster response

    // Calibrate gyro (keep same status message on screen)
    calibrate_gyro_bias();

    printf("Setup complete. Starting main loop...\n");
}

// ===================== DRAW ATTITUDE INDICATOR =====================
static void draw_attitude_indicator(float roll, float pitch, float heading) {
    uint16_t* fb = lcd_get_framebuffer();

    int center_x = LCD_WIDTH / 2;   // 160
    int center_y = LCD_HEIGHT / 2;  // 120

    // Calculate horizon line position and rotation
    float tilt = roll * 3.14159265f / 180.0f;
    float pitch_offset = -pitch * PITCH_PIX_PER_DEG;

    // Pre-compute trig values (used multiple times)
    float sin_tilt = sinf(tilt);
    float cos_tilt = cosf(tilt);

    // Optimized scanline-based horizon fill
    // Much faster than per-pixel rotation
    for (int y = 0; y < LCD_HEIGHT; y++) {
        int dy = y - center_y;

        // For this scanline, calculate the rotated y-position relative to horizon
        // We'll check where horizon crosses this scanline to determine fill
        for (int x = 0; x < LCD_WIDTH; x++) {
            int dx = x - center_x;

            // Rotate point by roll angle to get position relative to horizon
            float rotated_y = dx * sin_tilt + dy * cos_tilt;

            // Apply pitch offset
            rotated_y -= pitch_offset;

            // Sky above, ground below
            fb[y * LCD_WIDTH + x] = (rotated_y < 0) ? COLOR_SKY : COLOR_BROWN;
        }
    }

    // Draw white horizon line (3 pixels thick) - extend across entire display
    int half_width = 300;  // Long enough to reach edges at any rotation
    int dx = (int)(cos_tilt * (float)half_width);
    int dy = (int)(sin_tilt * (float)half_width);

    int x1 = center_x - dx;
    int y1 = center_y + dy + (int)pitch_offset;
    int x2 = center_x + dx;
    int y2 = center_y - dy + (int)pitch_offset;

    for (int t = -1; t <= 1; t++) {
        lcd_draw_line(x1, y1 + t, x2, y2 + t, COLOR_WHITE);
    }

    // Draw pitch ladder (10° increments)
    for (int pitch_deg = -30; pitch_deg <= 30; pitch_deg += 10) {
        if (pitch_deg == 0) continue;  // Skip zero (that's the horizon)

        float ladder_offset = -pitch_deg * PITCH_PIX_PER_DEG + pitch_offset;
        int ladder_width = (pitch_deg % 20 == 0) ? 40 : 25;  // Longer lines every 20°

        int ldx = (int)(cos_tilt * (float)ladder_width);
        int ldy = (int)(sin_tilt * (float)ladder_width);

        int lx1 = center_x - ldx;
        int ly1 = center_y + ldy + (int)ladder_offset;
        int lx2 = center_x + ldx;
        int ly2 = center_y - ldy + (int)ladder_offset;

        uint16_t ladder_color = (pitch_deg > 0) ? COLOR_WHITE : COLOR_WHITE;
        lcd_draw_line(lx1, ly1, lx2, ly2, ladder_color);
    }

    // Draw center aircraft symbol (fixed reference - yellow)
    // Wing bars
    lcd_draw_line(center_x - 50, center_y, center_x - 10, center_y, COLOR_YELLOW);
    lcd_draw_line(center_x + 10, center_y, center_x + 50, center_y, COLOR_YELLOW);
    lcd_draw_line(center_x - 50, center_y + 1, center_x - 10, center_y + 1, COLOR_YELLOW);
    lcd_draw_line(center_x + 10, center_y + 1, center_x + 50, center_y + 1, COLOR_YELLOW);

    // Center dot
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx*dx + dy*dy <= 4) {
                lcd_draw_pixel(center_x + dx, center_y + dy, COLOR_YELLOW);
            }
        }
    }

    // Draw heading overlay (on top of horizon)
    char buf[64];
#if USE_MAG
    snprintf(buf, sizeof(buf), "HDG:%.0f", heading);
#else
    snprintf(buf, sizeof(buf), "YAW:%.0f", heading);
#endif
    lcd_draw_string_scaled(5, 5, buf, COLOR_CYAN, COLOR_BLACK, 2);

    // Draw exit instruction (on top of horizon)
    lcd_draw_string(5, LCD_HEIGHT - 15, "KEY2: Exit", COLOR_WHITE, COLOR_BLACK);

    // Draw status icons (overlaid on sky, top-right corner)
    // Must be drawn here AFTER horizon fill to prevent being overwritten
    // Draw icons unconditionally for testing (with default false status if no telemetry)
    bool gps_status = telemetry_received ? latest_telemetry.status.gps : false;
    bool wifi_status = telemetry_received ? latest_telemetry.status.wifi : false;
    lcd_draw_gps_icon(268, 2, gps_status);
    lcd_draw_wifi_icon(296, 2, wifi_status);

    // Bank angle warning (>20 degrees)
    if (fabsf(roll) > 20.0f) {
        // Draw red background box (wider to accommodate angle text)
        int warning_y = LCD_HEIGHT - 35;
        int warning_height = 25;
        for (int y = warning_y; y < warning_y + warning_height; y++) {
            for (int x = center_x - 50; x < center_x + 50; x++) {
                if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
                    fb[y * LCD_WIDTH + x] = COLOR_RED;
                }
            }
        }
        // Draw bank angle warning text on red background
        char warn_buf[32];
        snprintf(warn_buf, sizeof(warn_buf), "BANK %.0f", fabsf(roll));
        lcd_draw_string_scaled(center_x - 48, LCD_HEIGHT - 30, warn_buf, COLOR_WHITE, COLOR_RED, 2);
    }
}

// ===================== MAIN RUN FUNCTION (for menu integration) =====================
void attitude_indicator_run(void) {
    printf("=== ATTITUDE INDICATOR selected ===\n");

    setup();

    uint64_t next_tick_us = to_us_since_boot(get_absolute_time());
    uint64_t last_print_ms = to_ms_since_boot(get_absolute_time());
    uint64_t last_update_us = 0;

    SensorData accel, gyro, mag;
    InputState input_state = {0};

    while (1) {
        // Read telemetry data (lightweight, just checks serial buffer)
        read_telemetry_data();

        // Check for exit button
        input_read(&input_state);
        if (input_just_pressed_key2(&input_state)) {
            printf("Exiting attitude indicator\n");
            break;
        }

        // Fixed-rate scheduler @ LOOP_HZ
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        if ((int64_t)(now_us - next_tick_us) < 0) {
            continue;
        }
        next_tick_us = now_us + LOOP_US;

        // Track actual delta time for the filter so integration matches real frame time
        float dt = 1.0f / (float)LOOP_HZ;
        if (last_update_us != 0) {
            float measured_dt = (now_us - last_update_us) / 1000000.0f;
            if (measured_dt < 0.001f) measured_dt = 0.001f;
            if (measured_dt > 0.05f) measured_dt = 0.05f;
            dt = measured_dt;
        }
        last_update_us = now_us;

        // Read IMU data (optimized burst read - ~30% faster)
        if (!icm20948_read_accel_gyro(&accel, &gyro)) {
            continue;
        }

        // Convert to physical units
        float ax = icm20948_accel_to_g(accel.x, accel_range);
        float ay = icm20948_accel_to_g(accel.y, accel_range);
        float az = icm20948_accel_to_g(accel.z, accel_range);

        float gx = icm20948_gyro_to_dps(gyro.x, gyro_range) - gx_bias;
        float gy = icm20948_gyro_to_dps(gyro.y, gyro_range) - gy_bias;
        float gz = icm20948_gyro_to_dps(gyro.z, gyro_range) - gz_bias;

        // Basic accel sanity check and gate: only trust accel if norm is near 1g
        float anorm2 = ax*ax + ay*ay + az*az;
        bool accel_valid = (anorm2 > 0.8f && anorm2 < 1.2f);

#if USE_MAG
        // Read magnetometer
        if (icm20948_read_mag(&mag)) {
            float mx = icm20948_mag_to_ut(mag.x);
            float my = icm20948_mag_to_ut(mag.y);
            float mz = icm20948_mag_to_ut(mag.z);

            // Swap X and Y axes for correct heading (common issue with sensor orientation)
            filter.inv_sample_freq = dt;       // q += qDot * dt
            filter.sample_freq = 1.0f / dt;
            madgwick_update_marg(&filter, gx, gy, gz,
                                 accel_valid ? ax : 0.0f,
                                 accel_valid ? ay : 0.0f,
                                 accel_valid ? az : 0.0f,
                                 my, mx, mz);
        } else {
            // Fallback to IMU-only if mag read fails
            filter.inv_sample_freq = dt;
            filter.sample_freq = 1.0f / dt;
            madgwick_update_imu(&filter, gx, gy, gz,
                                accel_valid ? ax : 0.0f,
                                accel_valid ? ay : 0.0f,
                                accel_valid ? az : 0.0f);
        }
#else
        filter.inv_sample_freq = dt;
        filter.sample_freq = 1.0f / dt;
        madgwick_update_imu(&filter, gx, gy, gz,
                            accel_valid ? ax : 0.0f,
                            accel_valid ? ay : 0.0f,
                            accel_valid ? az : 0.0f);
#endif

        // Get Euler angles
        float roll, pitch, yaw;
        madgwick_get_euler(&filter, &roll, &pitch, &yaw);

        float heading = wrap360(yaw);

        // UI smoothing (separate from filter)
        smooth_roll = smooth_roll * (1.0f - UI_SMOOTH) + roll * UI_SMOOTH;
        smooth_pitch = smooth_pitch * (1.0f - UI_SMOOTH) + pitch * UI_SMOOTH;

        smooth_roll = clampf(smooth_roll, -MAX_DISPLAY_DEG, MAX_DISPLAY_DEG);
        smooth_pitch = clampf(smooth_pitch, -MAX_DISPLAY_DEG, MAX_DISPLAY_DEG);

        // Draw attitude indicator at 50 Hz (every 2nd loop @ 100Hz)
        // 100Hz was too fast - LCD DMA transfer blocks the sensor reads
        static uint8_t display_counter = 0;
        if (++display_counter >= 2) {
            display_counter = 0;
            draw_attitude_indicator(smooth_roll, smooth_pitch, heading);
            lcd_flush();  // Single flush for everything
        }

        // Optional serial debug at ~2 Hz
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_print_ms > 500) {
            last_print_ms = now_ms;
            printf("roll=%.1f pitch=%.1f yaw/hdg=%.0f\n", roll, pitch, heading);
        }
    }
}
