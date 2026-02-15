/**
 * MPU-6050 6-axis IMU Library Implementation
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include "../include/mpu6050.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper functions for I2C communication
static int mpu6050_write_byte(int file, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(file, buf, 2) != 2) {
        perror("Failed to write to MPU-6050");
        return -1;
    }
    return 0;
}

static int mpu6050_read_bytes(int file, uint8_t reg, uint8_t *buffer, int length) {
    if (write(file, &reg, 1) != 1) {
        perror("Failed to write register address");
        return -1;
    }
    if (read(file, buffer, length) != length) {
        perror("Failed to read from MPU-6050");
        return -1;
    }
    return 0;
}

/**
 * Initialize the MPU-6050 sensor
 */
int mpu6050_init(void)
{
    int file;

    // Open I2C bus 1
    file = open("/dev/i2c-1", O_RDWR);
    if (file < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }

    // Set I2C slave address
    if (ioctl(file, I2C_SLAVE, MPU6050_ADDRESS) < 0) {
        perror("Failed to acquire bus access");
        close(file);
        return -1;
    }

    // Check WHO_AM_I register (should return 0x68)
    uint8_t who_am_i;
    if (mpu6050_read_bytes(file, MPU6050_REG_WHO_AM_I, &who_am_i, 1) < 0) {
        fprintf(stderr, "Failed to read WHO_AM_I register\n");
        close(file);
        return -1;
    }

    if (who_am_i != 0x68) {
        fprintf(stderr, "WHO_AM_I returned 0x%02X (expected 0x68)\n", who_am_i);
        close(file);
        return -1;
    }

    // Wake up MPU-6050 (reset PWR_MGMT_1 register)
    if (mpu6050_write_byte(file, MPU6050_REG_PWR_MGMT_1, 0x00) < 0) {
        fprintf(stderr, "Failed to wake up MPU-6050\n");
        close(file);
        return -1;
    }
    usleep(100000); // Wait 100ms for sensor to stabilize

    // Set sample rate divider to 0 for maximum 1kHz sample rate
    if (mpu6050_write_byte(file, MPU6050_REG_SMPLRT_DIV, 0x00) < 0) {
        fprintf(stderr, "Failed to set sample rate\n");
        close(file);
        return -1;
    }

    // Set DLPF to 184Hz bandwidth (0x01) for maximum responsiveness with vibration filtering
    // Config values: 0x00=260Hz, 0x01=184Hz, 0x02=98Hz, 0x03=42Hz, 0x04=20Hz, 0x05=10Hz, 0x06=5Hz
    if (mpu6050_write_byte(file, MPU6050_REG_CONFIG, 0x01) < 0) {
        fprintf(stderr, "Failed to set config\n");
        close(file);
        return -1;
    }

    // Set gyro range to ±250°/s
    if (mpu6050_write_byte(file, MPU6050_REG_GYRO_CONFIG, 0x00) < 0) {
        fprintf(stderr, "Failed to set gyro config\n");
        close(file);
        return -1;
    }

    // Set accelerometer range to ±2g
    if (mpu6050_write_byte(file, MPU6050_REG_ACCEL_CONFIG, 0x00) < 0) {
        fprintf(stderr, "Failed to set accel config\n");
        close(file);
        return -1;
    }

    return file;
}

/**
 * Read acceleration data from MPU-6050
 */
int mpu6050_read_accel(int file, float *x_g, float *y_g, float *z_g)
{
    uint8_t data[6];

    // Read 6 bytes starting from ACCEL_XOUT_H register
    if (mpu6050_read_bytes(file, MPU6050_REG_ACCEL_XOUT_H, data, 6) < 0) {
        return -1;
    }

    // Convert raw data to signed 16-bit integers (big-endian)
    int16_t x_raw = (int16_t)((data[0] << 8) | data[1]);
    int16_t y_raw = (int16_t)((data[2] << 8) | data[3]);
    int16_t z_raw = (int16_t)((data[4] << 8) | data[5]);

    // Scale to g values (±2g range = 16384 LSB/g)
    *x_g = x_raw / MPU6050_ACCEL_SCALE_2G;
    *y_g = y_raw / MPU6050_ACCEL_SCALE_2G;
    *z_g = z_raw / MPU6050_ACCEL_SCALE_2G;

    return 0;
}

/**
 * Read gyroscope data from MPU-6050
 */
int mpu6050_read_gyro(int file, float *gx, float *gy, float *gz)
{
    uint8_t data[6];

    // Read 6 bytes starting from GYRO_XOUT_H register
    if (mpu6050_read_bytes(file, MPU6050_REG_GYRO_XOUT_H, data, 6) < 0) {
        return -1;
    }

    // Convert raw data to signed 16-bit integers (big-endian)
    int16_t gx_raw = (int16_t)((data[0] << 8) | data[1]);
    int16_t gy_raw = (int16_t)((data[2] << 8) | data[3]);
    int16_t gz_raw = (int16_t)((data[4] << 8) | data[5]);

    // Scale to °/s (±250°/s range = 131 LSB/(°/s))
    *gx = gx_raw / MPU6050_GYRO_SCALE_250;
    *gy = gy_raw / MPU6050_GYRO_SCALE_250;
    *gz = gz_raw / MPU6050_GYRO_SCALE_250;

    return 0;
}

/**
 * Calculate pitch and roll angles from acceleration data
 *
 * Standard aviation conventions:
 * - Pitch: positive = nose up, negative = nose down
 * - Roll: positive = right wing down, negative = left wing down
 *
 * MPU-6050 orientation (matching ADXL345 implementation):
 * - X-axis: across wings (right positive)
 * - Y-axis: along fuselage (forward positive)
 * - Z-axis: vertical (down positive when level)
 */
void mpu6050_calculate_attitude(float x_g, float y_g, float z_g, float *pitch, float *roll)
{
    // Calculate pitch (rotation around X-axis)
    // Pitch uses Y and Z axes
    // INVERTED for HUD projection (positive pitch shows nose down on HUD)
    *pitch = atan2(x_g, z_g) * 180.0 / M_PI;

    // Calculate roll (rotation around Y-axis)
    // Roll uses X and Z axes
    *roll = -atan2(-y_g, z_g) * 180.0 / M_PI;
}

/**
 * Close MPU-6050 file descriptor
 */
void mpu6050_close(int file)
{
    if (file >= 0) {
        close(file);
    }
}
