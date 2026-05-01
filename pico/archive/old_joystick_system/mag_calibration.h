/**
 * Magnetometer Calibration for ICM-20948
 * Stores hard-iron offsets in flash memory to persist across reboots
 */

#ifndef MAG_CALIBRATION_H
#define MAG_CALIBRATION_H

#include <stdbool.h>

// Magnetometer calibration data (hard-iron offsets)
// Magic number must be first for easier verification
// Packed attribute ensures stable layout across compiler versions
typedef struct __attribute__((packed)) {
    uint32_t magic;  // Magic number to verify valid calibration data (must be first)
    float offset_x;
    float offset_y;
    float offset_z;
} MagCalibration;

/**
 * Load magnetometer calibration from flash
 * Returns true if valid calibration found, false otherwise
 */
bool mag_calibration_load(MagCalibration* cal);

/**
 * Save magnetometer calibration to flash
 * Returns true on success, false on failure
 */
bool mag_calibration_save(const MagCalibration* cal);

/**
 * Run magnetometer calibration routine
 * User rotates device in figure-8 pattern for ~30 seconds
 * Calculates and saves hard-iron offsets to flash
 */
void mag_calibration_run(void);

/**
 * Apply calibration offsets to raw magnetometer readings
 */
void mag_calibration_apply(float* mx, float* my, float* mz);

#endif // MAG_CALIBRATION_H
