/**
 * AHRS Core Module - Core 0 Implementation
 */

#include "ahrs_core.h"
#include "icm20948_sensor.h"
#include "madgwick_filter.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define D2R (M_PI / 180.0f)

// Shared attitude data (protected by mutex)
static AHRSAttitude shared_attitude = {0};
static mutex_t attitude_mutex;

// Control flags
static volatile bool ahrs_running = false;
static volatile bool ahrs_stop_requested = false;

// Core 0 entry point (AHRS processing loop)
static void ahrs_core0_entry(void);

// ── Public API ────────────────────────────────────────────────────────────────

void ahrs_core_start(void) {
    // Initialize mutex
    mutex_init(&attitude_mutex);

    // Reset state
    ahrs_stop_requested = false;
    ahrs_running = false;

    // Clear shared data
    mutex_enter_blocking(&attitude_mutex);
    memset(&shared_attitude, 0, sizeof(shared_attitude));
    mutex_exit(&attitude_mutex);

    printf("[AHRS] Launching Core 0...\n");

    // Launch AHRS on Core 0
    multicore_launch_core1(ahrs_core0_entry);

    // Wait for Core 0 to signal it's running
    uint32_t timeout = 0;
    while (!ahrs_running && timeout++ < 100) {
        sleep_ms(10);
    }

    if (ahrs_running) {
        printf("[AHRS] Core 0 started successfully\n");
    } else {
        printf("[AHRS] ERROR: Core 0 failed to start\n");
    }
}

void ahrs_core_stop(void) {
    printf("[AHRS] Stopping Core 0...\n");
    ahrs_stop_requested = true;

    // Wait for Core 0 to stop (max 1 second)
    uint32_t timeout = 0;
    while (ahrs_running && timeout++ < 100) {
        sleep_ms(10);
    }

    if (!ahrs_running) {
        printf("[AHRS] Core 0 stopped successfully\n");
    } else {
        printf("[AHRS] WARNING: Core 0 did not stop cleanly\n");
    }

    // Reset multicore (safe shutdown)
    multicore_reset_core1();
}

bool ahrs_core_get_attitude(AHRSAttitude* attitude) {
    if (!attitude) return false;

    mutex_enter_blocking(&attitude_mutex);
    *attitude = shared_attitude;
    mutex_exit(&attitude_mutex);

    return shared_attitude.valid;
}

bool ahrs_core_is_healthy(void) {
    return ahrs_running && shared_attitude.valid;
}

void ahrs_core_reset_filter(void) {
    // TODO: Implement filter reset via inter-core FIFO
    printf("[AHRS] Filter reset not yet implemented\n");
}

// ── Core 0 AHRS Processing Loop ───────────────────────────────────────────────

static void ahrs_core0_entry(void) {
    printf("[Core 0] AHRS starting...\n");

    // Initialize sensor
    if (!icm20948_init()) {
        printf("[Core 0] ERROR: ICM20948 init failed!\n");
        mutex_enter_blocking(&attitude_mutex);
        shared_attitude.valid = false;
        mutex_exit(&attitude_mutex);
        return;
    }

    printf("[Core 0] ICM20948 initialized\n");

    // Initialize Madgwick filter
    MadgwickFilter filter;
    madgwick_init(&filter, 100.0f, 0.02f);  // 100 Hz, beta=0.02

    SensorData accel, gyro;

    // ── Calibration Phase ─────────────────────────────────────────────────────

    printf("[Core 0] Calibrating (200 samples)...\n");
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
    }

    gyro_bias_x /= CAL_N;
    gyro_bias_y /= CAL_N;
    gyro_bias_z /= CAL_N;
    accel_bias_x /= CAL_N;
    accel_bias_y /= CAL_N;
    accel_bias_z /= CAL_N;
    accel_bias_z -= 1.0f;  // Keep gravity in Z

    printf("[Core 0] Gyro bias: X=%.3f Y=%.3f Z=%.3f deg/s\n", gyro_bias_x, gyro_bias_y, gyro_bias_z);
    printf("[Core 0] Accel bias: X=%.3f Y=%.3f Z=%.3f g\n", accel_bias_x, accel_bias_y, accel_bias_z);

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

        printf("[Core 0] Initial attitude: Roll=%.1f° Pitch=%.1f°\n", r * 180.0f / M_PI, p * 180.0f / M_PI);
    }

    // Output smoothing
    float smooth_roll = 0.0f;
    float smooth_pitch = 0.0f;
    const float smooth_alpha = 0.15f;  // Lower = smoother (0.15 = gentle smoothing)

    // Timing
    absolute_time_t last_update = get_absolute_time();
    absolute_time_t last_diag = get_absolute_time();
    float dt_min = 1.0f, dt_max = 0.0f, dt_sum = 0.0f;
    uint32_t dt_samples = 0;
    uint32_t update_count = 0;

    printf("[Core 0] Calibration complete, starting AHRS loop...\n");
    ahrs_running = true;

    // Mark as calibrated
    mutex_enter_blocking(&attitude_mutex);
    shared_attitude.calibrated = true;
    shared_attitude.gyro_bias_x = gyro_bias_x;
    shared_attitude.gyro_bias_y = gyro_bias_y;
    shared_attitude.gyro_bias_z = gyro_bias_z;
    mutex_exit(&attitude_mutex);

    // ── Main AHRS Loop ────────────────────────────────────────────────────────

    while (!ahrs_stop_requested) {
        // Read sensors
        if (!icm20948_read_accel(&accel) || !icm20948_read_gyro(&gyro)) {
            continue;
        }

        // Calculate delta time
        absolute_time_t now = get_absolute_time();
        int64_t dt_us = absolute_time_diff_us(last_update, now);
        last_update = now;
        float dt = (float)dt_us / 1000000.0f;
        if (dt <= 0.0f || dt > 0.2f) dt = 0.01f;

        // Collect timing statistics
        if (dt > 0.0f && dt < 0.2f) {
            if (dt < dt_min) dt_min = dt;
            if (dt > dt_max) dt_max = dt;
            dt_sum += dt;
            dt_samples++;
        }

        // Convert sensor readings
        float ax = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G) - accel_bias_x;
        float ay = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G) - accel_bias_y;
        float az = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G) - accel_bias_z;

        float gx_raw = icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
        float gy_raw = icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
        float gz_raw = icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);

        float gx_dps = gx_raw - gyro_bias_x;
        float gy_dps = gy_raw - gyro_bias_y;
        float gz_dps = gz_raw - gyro_bias_z;

        // Sanity checks
        if (!isfinite(gx_dps) || !isfinite(gy_dps) || !isfinite(gz_dps)) continue;
        if (fabsf(gx_dps) > 2000.0f || fabsf(gy_dps) > 2000.0f || fabsf(gz_dps) > 2000.0f) continue;

        // Detect stationary state
        float acc_norm = sqrtf(ax*ax + ay*ay + az*az);
        bool stationary = isfinite(acc_norm) &&
                          fabsf(acc_norm - 1.0f) < 0.05f &&
                          fabsf(gx_dps) < 0.5f &&
                          fabsf(gy_dps) < 0.5f &&
                          fabsf(gz_dps) < 0.5f;

        // Adaptive bias correction when stationary
        if (stationary) {
            float alpha = dt * 0.5f;
            if (alpha > 0.1f) alpha = 0.1f;

            gyro_bias_x = (1.0f - alpha) * gyro_bias_x + alpha * gx_raw;
            gyro_bias_y = (1.0f - alpha) * gyro_bias_y + alpha * gy_raw;
            gyro_bias_z = (1.0f - alpha) * gyro_bias_z + alpha * gz_raw;

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

        // Update Madgwick filter
        filter.sample_freq = 1.0f / dt;
        filter.inv_sample_freq = dt;
        madgwick_update_imu(&filter, gx, gy, gz, ax, ay, az);

        // Get attitude (invert pitch so nose-up is positive)
        float roll = -madgwick_get_roll_deg(&filter);
        float pitch = -madgwick_get_pitch_deg(&filter);

        // Sanity check
        if (!isfinite(roll) || !isfinite(pitch)) {
            madgwick_init(&filter, 100.0f, 0.02f);
            continue;
        }

        // Apply output smoothing
        smooth_roll = smooth_roll * (1.0f - smooth_alpha) + roll * smooth_alpha;
        smooth_pitch = smooth_pitch * (1.0f - smooth_alpha) + pitch * smooth_alpha;

        roll = smooth_roll;
        pitch = smooth_pitch;

        // Update shared data (critical section)
        mutex_enter_blocking(&attitude_mutex);
        shared_attitude.roll = roll;
        shared_attitude.pitch = pitch;
        shared_attitude.yaw = 0.0f;  // Not yet implemented
        shared_attitude.valid = true;
        shared_attitude.stationary = stationary;
        shared_attitude.gyro_bias_x = gyro_bias_x;
        shared_attitude.gyro_bias_y = gyro_bias_y;
        shared_attitude.gyro_bias_z = gyro_bias_z;
        shared_attitude.update_count = ++update_count;
        shared_attitude.timestamp_us = to_us_since_boot(now);

        if (dt_samples > 0) {
            shared_attitude.loop_rate_hz = 1.0f / (dt_sum / dt_samples);
            shared_attitude.timing_jitter_ms = (dt_max - dt_min) * 1000.0f;
        }
        mutex_exit(&attitude_mutex);

        // Print diagnostics every 5 seconds
        if (absolute_time_diff_us(last_diag, now) > 5000000) {
            if (dt_samples > 0) {
                float dt_avg = dt_sum / dt_samples;
                float jitter_ms = (dt_max - dt_min) * 1000.0f;
                printf("[Core 0] Roll:%+7.2f Pitch:%+7.2f | Rate:%.1fHz Jitter:%.3fms | %s\n",
                       roll, pitch, 1.0f / dt_avg, jitter_ms, stationary ? "CAL" : "MOV");
            }
            dt_min = 1.0f; dt_max = 0.0f; dt_sum = 0.0f; dt_samples = 0;
            last_diag = now;
        }
    }

    // Shutdown
    printf("[Core 0] AHRS stopping...\n");
    icm20948_sleep();

    mutex_enter_blocking(&attitude_mutex);
    shared_attitude.valid = false;
    mutex_exit(&attitude_mutex);

    ahrs_running = false;
    printf("[Core 0] AHRS stopped\n");
}
