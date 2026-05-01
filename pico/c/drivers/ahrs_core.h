/**
 * AHRS Core Module - Dedicated Core 0 AHRS Processing
 *
 * This module runs the AHRS (Attitude & Heading Reference System) on a dedicated
 * CPU core to ensure timing isolation from WiFi, Bluetooth, and LCD operations.
 *
 * Architecture:
 * - Core 0: AHRS loop (this module) - 100 Hz sensor reads, Madgwick filter
 * - Core 1: Main application - WiFi, BT, LCD, touch, menu system
 * - Communication: Mutex-protected shared memory
 */

#ifndef AHRS_CORE_H
#define AHRS_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Shared attitude data structure (updated by Core 0, read by Core 1)
typedef struct {
    float roll;           // Roll angle in degrees (-180 to +180)
    float pitch;          // Pitch angle in degrees (-90 to +90)
    float yaw;            // Yaw angle in degrees (0 to 360) - reserved for magnetometer

    // Gyro bias tracking
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;

    // Status flags
    bool valid;           // True if AHRS is running and data is valid
    bool stationary;      // True if device is stationary (calibrating)
    bool calibrated;      // True if initial calibration complete

    // Diagnostics
    uint32_t update_count;    // Total AHRS updates since start
    float loop_rate_hz;       // Actual AHRS loop rate
    float timing_jitter_ms;   // Timing jitter (max - min dt)

    // Timestamp
    uint64_t timestamp_us;    // Microsecond timestamp of last update
} AHRSAttitude;

/**
 * Start the AHRS core on Core 0
 * This function launches the AHRS processing loop on the second CPU core.
 * It performs sensor initialization and calibration before starting the main loop.
 *
 * Must be called from Core 1 (main core).
 * Returns immediately after launching Core 0.
 */
void ahrs_core_start(void);

/**
 * Stop the AHRS core
 * Signals Core 0 to stop processing and puts the sensor to sleep.
 * Blocks until Core 0 has safely shut down.
 */
void ahrs_core_stop(void);

/**
 * Get current attitude data (thread-safe)
 * Copies the current attitude data to the provided structure.
 * Uses mutex protection to ensure atomic read.
 *
 * Returns: true if data is valid, false if AHRS is not running
 */
bool ahrs_core_get_attitude(AHRSAttitude* attitude);

/**
 * Check if AHRS core is running and healthy
 * Returns: true if Core 0 is actively updating attitude data
 */
bool ahrs_core_is_healthy(void);

/**
 * Reset AHRS filter to initial state
 * Useful after large disturbances or orientation changes
 */
void ahrs_core_reset_filter(void);

#endif // AHRS_CORE_H
