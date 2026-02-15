/**
 * Madgwick AHRS Filter
 *
 * Sensor fusion algorithm for 9-DOF IMU (accelerometer, gyroscope, magnetometer)
 * Estimates orientation as a quaternion using gradient descent optimization.
 *
 * Reference: S.O.H. Madgwick, "An efficient orientation filter for inertial
 * and inertial/magnetic sensor arrays," University of Bristol, 2010.
 */

#ifndef MADGWICK_FILTER_H
#define MADGWICK_FILTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Quaternion representation (w, x, y, z)
 * Represents 3D rotation: q = w + xi + yj + zk
 */
typedef struct {
    float q0;  // w (scalar part)
    float q1;  // x (vector i)
    float q2;  // y (vector j)
    float q3;  // z (vector k)
} Quaternion;

/**
 * Madgwick filter state
 */
typedef struct {
    Quaternion q;           // Current orientation quaternion
    float beta;             // Filter gain (convergence rate)
    float sample_freq;      // Sampling frequency (Hz)
    float inv_sample_freq;  // 1 / sample_freq (cached for performance)
} MadgwickFilter;

/**
 * Initialize Madgwick filter
 *
 * @param filter Pointer to filter structure
 * @param sample_freq Sampling frequency in Hz (e.g., 200.0)
 * @param beta Filter gain (default: 0.1). Higher = faster convergence, more noise.
 */
void madgwick_init(MadgwickFilter* filter, float sample_freq, float beta);

/**
 * Update filter with 9-DOF sensor data
 *
 * @param filter Pointer to filter structure
 * @param gx Gyroscope X (rad/s)
 * @param gy Gyroscope Y (rad/s)
 * @param gz Gyroscope Z (rad/s)
 * @param ax Accelerometer X (g)
 * @param ay Accelerometer Y (g)
 * @param az Accelerometer Z (g)
 * @param mx Magnetometer X (µT)
 * @param my Magnetometer Y (µT)
 * @param mz Magnetometer Z (µT)
 */
void madgwick_update(MadgwickFilter* filter,
                    float gx, float gy, float gz,
                    float ax, float ay, float az,
                    float mx, float my, float mz);

/**
 * Update filter with 6-DOF sensor data (no magnetometer)
 * Yaw will drift over time without magnetometer correction.
 *
 * @param filter Pointer to filter structure
 * @param gx Gyroscope X (rad/s)
 * @param gy Gyroscope Y (rad/s)
 * @param gz Gyroscope Z (rad/s)
 * @param ax Accelerometer X (g)
 * @param ay Accelerometer Y (g)
 * @param az Accelerometer Z (g)
 */
void madgwick_update_imu(MadgwickFilter* filter,
                        float gx, float gy, float gz,
                        float ax, float ay, float az);

/**
 * Convert quaternion to Euler angles (roll, pitch, yaw)
 *
 * @param q Pointer to quaternion
 * @param roll Output: roll angle in radians (rotation around X-axis)
 * @param pitch Output: pitch angle in radians (rotation around Y-axis)
 * @param yaw Output: yaw angle in radians (rotation around Z-axis)
 */
void quaternion_to_euler(const Quaternion* q, float* roll, float* pitch, float* yaw);

/**
 * Get roll angle in degrees
 */
float madgwick_get_roll_deg(const MadgwickFilter* filter);

/**
 * Get pitch angle in degrees
 */
float madgwick_get_pitch_deg(const MadgwickFilter* filter);

/**
 * Get yaw angle in degrees (heading)
 */
float madgwick_get_yaw_deg(const MadgwickFilter* filter);

#endif // MADGWICK_FILTER_H
