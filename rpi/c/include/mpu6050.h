/**
 * MPU-6050 6-axis IMU Library
 * I2C interface for reading accelerometer and gyroscope data
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

// MPU-6050 I2C address
#define MPU6050_ADDRESS 0x68  // AD0 pin low (default)

// MPU-6050 register addresses
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H  0x43

// Scale factors
#define MPU6050_ACCEL_SCALE_2G  16384.0  // LSB/g for ±2g range
#define MPU6050_GYRO_SCALE_250  131.0    // LSB/(°/s) for ±250°/s range

/**
 * Initialize the MPU-6050 sensor
 * Returns: file descriptor or -1 on error
 */
int mpu6050_init(void);

/**
 * Read raw acceleration data from MPU-6050
 * Returns: 0 on success, -1 on error
 */
int mpu6050_read_accel(int file, float *x_g, float *y_g, float *z_g);

/**
 * Read raw gyroscope data from MPU-6050
 * Returns: 0 on success, -1 on error
 */
int mpu6050_read_gyro(int file, float *gx, float *gy, float *gz);

/**
 * Calculate pitch and roll angles from acceleration data
 *
 * Aviation conventions:
 * - Pitch: positive = nose up, negative = nose down
 * - Roll: positive = right wing down, negative = left wing down
 *
 * x_g, y_g, z_g: acceleration in g-forces
 * pitch, roll: output angles in degrees
 */
void mpu6050_calculate_attitude(float x_g, float y_g, float z_g, float *pitch, float *roll);

/**
 * Close MPU-6050 file descriptor
 */
void mpu6050_close(int file);

#endif // MPU6050_H
