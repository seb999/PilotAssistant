#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "st7789_lcd.h"
#include "icm20948_sensor.h"
#include "madgwick_filter.h"

#define LED_PIN 25
#define D2R (M_PI / 180.0f)
#define PX_PER_DEG 2.5f

// Draw artificial horizon background
static void draw_horizon_bg(float roll_rad, float pitch_px) {
    const int16_t cx = 160, cy = 120;
    const float sin_r = sinf(roll_rad);
    const float cos_r = cosf(roll_rad);

    // Sky blue and darker brown earth
    const uint16_t sky_color = 0x4D9F;
    const uint16_t earth_color = 0x6180;  // Darker brown

    for (int16_t py = 0; py < LCD_HEIGHT; py++) {
        for (int16_t px = 0; px < LCD_WIDTH; px++) {
            float dx = px - cx;
            float dy = py - cy;
            float rotated_y = dx * sin_r + dy * cos_r;
            float world_y = rotated_y - pitch_px;

            uint16_t color = (world_y < 0) ? sky_color : earth_color;
            lcd_draw_pixel(px, py, color);
        }
    }
}

// Draw pitch ladder
static void draw_pitch_ladder(float roll_rad, float pitch_deg) {
    const int16_t cx = 160, cy = 120;
    const float sin_r = sinf(roll_rad);
    const float cos_r = cosf(roll_rad);

    // Draw pitch lines every 10 degrees - lines rotate with bank to stay parallel to horizon
    for (int p = -60; p <= 60; p += 10) {
        if (p == 0) continue;  // Skip horizon (already drawn by background)

        float pitch_offset = (pitch_deg - p) * PX_PER_DEG;

        // Draw line from left to right, rotated by bank angle
        int16_t x1 = cx - 40, x2 = cx + 40;

        // Rotate the line endpoints around center based on bank angle (negate sin for correct direction)
        int16_t y1 = cy + (int16_t)(-(x1 - cx) * sin_r + pitch_offset * cos_r);
        int16_t y2 = cy + (int16_t)(-(x2 - cx) * sin_r + pitch_offset * cos_r);

        if ((y1 >= 0 && y1 < LCD_HEIGHT) || (y2 >= 0 && y2 < LCD_HEIGHT)) {
            lcd_draw_line(x1, y1, x2, y2, COLOR_WHITE);

            // Label at right end
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%d", p);
            if (p > 0) {
                lcd_draw_string(x2 + 5, y2 - 3, lbl, COLOR_CYAN, COLOR_BLACK);
            } else {
                lcd_draw_string(x2 + 5, y2 - 3, lbl, COLOR_YELLOW, COLOR_BLACK);
            }
        }
    }
}

// Draw bank angle arc
static void draw_bank_arc(float roll_deg) {
    const int16_t cx = 160, cy = 120;
    const int radius = 90;

    // Draw arc markings at -60, -45, -30, -20, -10, 0, +10, +20, +30, +45, +60
    int marks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int i = 0; i < 11; i++) {
        float angle = marks[i] * D2R;
        int16_t x = cx + (int16_t)(radius * sinf(angle));
        int16_t y = cy - (int16_t)(radius * cosf(angle));

        uint16_t color = (marks[i] == 0) ? COLOR_YELLOW : COLOR_WHITE;
        lcd_fill_rect(x - 1, y - 1, 3, 3, color);
    }

    // Draw current bank indicator (red triangle)
    float angle = roll_deg * D2R;
    int16_t x = cx + (int16_t)(radius * sinf(angle));
    int16_t y = cy - (int16_t)(radius * cosf(angle));
    lcd_fill_rect(x - 2, y - 2, 5, 5, COLOR_RED);
}

int main(void) {
    set_sys_clock_khz(200000, true);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    stdio_init_all();
    sleep_ms(2000);

    printf("\n\n=== Standalone AHRS Test ===\n");

    lcd_init();
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(40, 100, "AHRS TEST", COLOR_CYAN, COLOR_BLACK, 3);
    lcd_draw_string(60, 140, "Initializing sensor...", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    sleep_ms(1000);

    if (!icm20948_init()) {
        printf("ERROR: ICM20948 init failed!\n");
        lcd_clear(COLOR_BLACK);
        lcd_draw_string_scaled(20, 60, "SENSOR ERROR", COLOR_RED, COLOR_BLACK, 3);
        lcd_flush();
        while (1) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            sleep_ms(500);
        }
    }

    printf("ICM20948 initialized successfully\n");

    // Initialize Madgwick filter (lower beta = smoother, less accelerometer correction)
    MadgwickFilter filter;
    madgwick_init(&filter, 100.0f, 0.02f);  // 100 Hz, beta = 0.02 for smoother response

    SensorData accel, gyro;

    // === CALIBRATION: 200 samples (~2 seconds) ===
    printf("Calibrating gyro and accel (keep device stationary)...\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(40, 100, "CALIBRATING...", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(20, 120, "Keep device stationary", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();

    float gyro_bias_x = 0, gyro_bias_y = 0, gyro_bias_z = 0;
    float accel_bias_x = 0, accel_bias_y = 0, accel_bias_z = 0;
    const int CAL_N = 200;

    for (int i = 0; i < CAL_N; i++) {
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

        if (i % 20 == 0) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
        }
    }

    gyro_bias_x /= CAL_N;
    gyro_bias_y /= CAL_N;
    gyro_bias_z /= CAL_N;
    accel_bias_x /= CAL_N;
    accel_bias_y /= CAL_N;
    accel_bias_z /= CAL_N;
    accel_bias_z -= 1.0f;  // Keep gravity in Z

    printf("Gyro bias: X=%.3f Y=%.3f Z=%.3f deg/s\n", gyro_bias_x, gyro_bias_y, gyro_bias_z);
    printf("Accel bias: X=%.3f Y=%.3f Z=%.3f g\n", accel_bias_x, accel_bias_y, accel_bias_z);

    // Seed quaternion from initial accelerometer reading
    if (icm20948_read_accel(&accel)) {
        float ax0 = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G) - accel_bias_x;
        float ay0 = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G) - accel_bias_y;
        float az0 = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G) - accel_bias_z;

        float r = atan2f(ay0, az0);
        float p = atan2f(-ax0, sqrtf(ay0*ay0 + az0*az0));

        filter.q.q0 =  cosf(r/2) * cosf(p/2);
        filter.q.q1 =  sinf(r/2) * cosf(p/2);
        filter.q.q2 =  cosf(r/2) * sinf(p/2);
        filter.q.q3 = -sinf(r/2) * sinf(p/2);

        printf("Initial attitude: Roll=%.1f° Pitch=%.1f°\n", r * 180.0f / M_PI, p * 180.0f / M_PI);
    }

    printf("Calibration complete. Starting AHRS...\n");
    gpio_put(LED_PIN, 1);

    absolute_time_t last_update = get_absolute_time();
    absolute_time_t last_print = get_absolute_time();
    uint8_t render_counter = 0;

    // Output smoothing filter
    float smooth_roll = 0.0f;
    float smooth_pitch = 0.0f;
    const float smooth_alpha = 0.3f;  // 0 = no smoothing, 1 = instant

    while (true) {
        if (!icm20948_read_accel(&accel) || !icm20948_read_gyro(&gyro)) {
            continue;
        }

        // Calculate delta time
        absolute_time_t now = get_absolute_time();
        int64_t dt_us = absolute_time_diff_us(last_update, now);
        last_update = now;
        float dt = (float)dt_us / 1000000.0f;
        if (dt <= 0.0f || dt > 0.2f) dt = 0.01f;

        // Convert accelerometer readings
        float ax = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G) - accel_bias_x;
        float ay = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G) - accel_bias_y;
        float az = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G) - accel_bias_z;

        // Convert gyro readings and subtract bias
        float gx_raw = icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
        float gy_raw = icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
        float gz_raw = icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);

        float gx_dps = gx_raw - gyro_bias_x;
        float gy_dps = gy_raw - gyro_bias_y;
        float gz_dps = gz_raw - gyro_bias_z;

        // Sanity checks
        if (!isfinite(gx_dps) || !isfinite(gy_dps) || !isfinite(gz_dps)) continue;
        if (fabsf(gx_dps) > 2000.0f || fabsf(gy_dps) > 2000.0f || fabsf(gz_dps) > 2000.0f) continue;

        // Adaptive bias correction when stationary
        float acc_norm = sqrtf(ax*ax + ay*ay + az*az);
        bool stationary = isfinite(acc_norm) &&
                          fabsf(acc_norm - 1.0f) < 0.05f &&  // Very tight accel norm check
                          fabsf(gx_dps) < 0.5f &&             // Very low gyro activity
                          fabsf(gy_dps) < 0.5f &&
                          fabsf(gz_dps) < 0.5f;

        if (stationary) {
            // Aggressive bias tracking
            float alpha = dt * 0.5f;  // Fast adaptation
            if (alpha > 0.1f) alpha = 0.1f;

            gyro_bias_x = (1.0f - alpha) * gyro_bias_x + alpha * gx_raw;
            gyro_bias_y = (1.0f - alpha) * gyro_bias_y + alpha * gy_raw;
            gyro_bias_z = (1.0f - alpha) * gyro_bias_z + alpha * gz_raw;

            // Recalculate after bias update
            gx_dps = gx_raw - gyro_bias_x;
            gy_dps = gy_raw - gyro_bias_y;
            gz_dps = gz_raw - gyro_bias_z;
        }

        // Dead zone to eliminate tiny drift
        if (fabsf(gx_dps) < 0.5f) gx_dps = 0.0f;
        if (fabsf(gy_dps) < 0.5f) gy_dps = 0.0f;
        if (fabsf(gz_dps) < 0.5f) gz_dps = 0.0f;

        // Convert to rad/s
        float gx = gx_dps * D2R;
        float gy = gy_dps * D2R;
        float gz = gz_dps * D2R;

        // Update filter
        filter.sample_freq = 1.0f / dt;
        filter.inv_sample_freq = dt;
        madgwick_update_imu(&filter, gx, gy, gz, ax, ay, az);

        // Get attitude (invert pitch so nose-up is positive)
        float roll = -madgwick_get_roll_deg(&filter);
        float pitch = -madgwick_get_pitch_deg(&filter);

        // Sanity check
        if (!isfinite(roll) || !isfinite(pitch)) {
            printf("ERROR: Invalid attitude, resetting filter\n");
            madgwick_init(&filter, 100.0f, 0.02f);
            continue;
        }

        // Apply output smoothing
        smooth_roll = smooth_roll * (1.0f - smooth_alpha) + roll * smooth_alpha;
        smooth_pitch = smooth_pitch * (1.0f - smooth_alpha) + pitch * smooth_alpha;

        // Use smoothed values for display
        roll = smooth_roll;
        pitch = smooth_pitch;

        // Print to serial every 500ms
        if (absolute_time_diff_us(last_print, now) > 500000) {
            printf("Roll: %+7.2f°  Pitch: %+7.2f°  ", roll, pitch);
            printf("Gyro: X=%+6.2f Y=%+6.2f Z=%+6.2f  ", gx_dps, gy_dps, gz_dps);
            printf("Status: %s\n", stationary ? "STATIONARY" : "MOVING    ");
            last_print = now;
        }

        // Render at ~25 Hz (every 4th iteration assuming ~100Hz sensor rate)
        if (++render_counter < 4) continue;
        render_counter = 0;

        // Draw AHRS display
        float roll_rad = roll * D2R;
        float pitch_px = -pitch * PX_PER_DEG;

        draw_horizon_bg(roll_rad, pitch_px);
        draw_pitch_ladder(roll_rad, pitch);
        draw_bank_arc(roll);

        // Aircraft reference symbol (yellow)
        const int16_t cx = 160, cy = 120;
        lcd_draw_line(cx - 50, cy, cx - 10, cy, COLOR_YELLOW);
        lcd_draw_line(cx + 10, cy, cx + 50, cy, COLOR_YELLOW);
        lcd_draw_line(cx - 50, cy + 1, cx - 10, cy + 1, COLOR_YELLOW);
        lcd_draw_line(cx + 10, cy + 1, cx + 50, cy + 1, COLOR_YELLOW);
        lcd_fill_rect(cx - 3, cy - 3, 7, 7, COLOR_YELLOW);

        // Attitude display at top
        char buf[32];
        snprintf(buf, sizeof(buf), "R%+6.1f  P%+6.1f", roll, pitch);
        lcd_fill_rect(0, 0, 150, 12, COLOR_BLACK);
        lcd_draw_string(4, 4, buf, COLOR_WHITE, COLOR_BLACK);

        // Bias tracking indicator
        if (stationary) {
            lcd_draw_string(250, 4, "CAL", COLOR_GREEN, COLOR_BLACK);
        }

        lcd_flush();

        // Blink LED to show activity
        static uint32_t led_counter = 0;
        if (++led_counter >= 50) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            led_counter = 0;
        }
    }

    return 0;
}
