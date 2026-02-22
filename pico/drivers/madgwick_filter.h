/**
 * Madgwick AHRS Filter
 * Attitude and Heading Reference System using IMU/MARG sensor fusion
 */

#ifndef MADGWICK_H
#define MADGWICK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float q0, q1, q2, q3;  // Quaternion elements
    float beta;             // Filter gain
    float sample_freq;      // Sample frequency (Hz)
    float inv_sample_freq;  // 1.0 / sample_freq
} MadgwickFilter;

/**
 * Initialize the Madgwick filter
 * sample_freq: Update rate in Hz (e.g., 100.0)
 * beta: Filter gain (default 0.1, higher = faster convergence but more noise)
 */
void madgwick_init(MadgwickFilter* filter, float sample_freq, float beta);

/**
 * Update filter with IMU data (6DOF - no magnetometer)
 * gx, gy, gz: Gyroscope in degrees/sec
 * ax, ay, az: Accelerometer in g
 */
void madgwick_update_imu(MadgwickFilter* filter,
                         float gx, float gy, float gz,
                         float ax, float ay, float az);

/**
 * Update filter with MARG data (9DOF - with magnetometer)
 * gx, gy, gz: Gyroscope in degrees/sec
 * ax, ay, az: Accelerometer in g
 * mx, my, mz: Magnetometer in microTesla
 */
void madgwick_update_marg(MadgwickFilter* filter,
                          float gx, float gy, float gz,
                          float ax, float ay, float az,
                          float mx, float my, float mz);

/**
 * Get Euler angles from quaternion
 * Returns: roll, pitch, yaw in degrees
 */
void madgwick_get_euler(MadgwickFilter* filter, float* roll, float* pitch, float* yaw);

/**
 * Get quaternion directly
 */
void madgwick_get_quaternion(MadgwickFilter* filter, float* q0, float* q1, float* q2, float* q3);

#endif // MADGWICK_H
