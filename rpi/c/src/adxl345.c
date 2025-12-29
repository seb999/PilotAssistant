/**
 * ADXL345 Accelerometer Library Implementation
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
#include "../include/adxl345.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Initialize the ADXL345 sensor
 */
int adxl345_init(void)
{
    int file;

    // Open I2C bus 1
    file = open("/dev/i2c-1", O_RDWR);
    if (file < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }

    // Set I2C slave address
    if (ioctl(file, I2C_SLAVE, ADXL345_ADDRESS) < 0) {
        perror("Failed to acquire bus access");
        close(file);
        return -1;
    }

    // Set output data rate to 800 Hz (maximum speed for low latency)
    // BW_RATE register: 0x0D = 800 Hz output data rate
    uint8_t config[2] = {ADXL345_BW_RATE, 0x0D};
    if (write(file, config, 2) != 2) {
        perror("Failed to write to BW_RATE register");
        close(file);
        return -1;
    }

    // Set data format (±2g, full resolution)
    config[0] = ADXL345_DATA_FORMAT;
    config[1] = 0x08;
    if (write(file, config, 2) != 2) {
        perror("Failed to write to DATA_FORMAT register");
        close(file);
        return -1;
    }

    // Wake up the ADXL345 (set Measure bit to 1)
    config[0] = ADXL345_POWER_CTL;
    config[1] = 0x08;
    if (write(file, config, 2) != 2) {
        perror("Failed to write to POWER_CTL register");
        close(file);
        return -1;
    }

    // Small delay to let sensor stabilize
    usleep(10000);

    return file;
}

/**
 * Read acceleration data from ADXL345
 */
int adxl345_read_axes(int file, float *x_g, float *y_g, float *z_g)
{
    uint8_t data[6];

    // Read 6 bytes starting from DATAX0 register
    if (write(file, (uint8_t[]){ADXL345_DATAX0}, 1) != 1) {
        perror("Failed to set register address");
        return -1;
    }

    if (read(file, data, 6) != 6) {
        perror("Failed to read acceleration data");
        return -1;
    }

    // Convert raw data to signed 16-bit integers (little-endian)
    int16_t x_raw = (int16_t)(data[0] | (data[1] << 8));
    int16_t y_raw = (int16_t)(data[2] | (data[3] << 8));
    int16_t z_raw = (int16_t)(data[4] | (data[5] << 8));

    // Scale to g values
    *x_g = x_raw / SCALE_FACTOR;
    *y_g = y_raw / SCALE_FACTOR;
    *z_g = z_raw / SCALE_FACTOR;

    return 0;
}

/**
 * Calculate pitch and roll angles from acceleration data
 *
 * Standard aviation conventions:
 * - Pitch: positive = nose up, negative = nose down
 * - Roll: positive = right wing down, negative = left wing down
 *
 * ADXL345 orientation (matching Python implementation):
 * - X-axis: across wings (right positive)
 * - Y-axis: along fuselage (forward positive)
 * - Z-axis: vertical (down positive when level)
 */
void adxl345_calculate_attitude(float x_g, float y_g, float z_g, float *pitch, float *roll)
{
    // Calculate pitch (rotation around X-axis)
    // Pitch uses Y and Z axes
    // INVERTED for HUD projection (positive pitch shows nose down on HUD)
    *pitch = -atan2(-y_g, z_g) * 180.0 / M_PI;

    // Calculate roll (rotation around Y-axis)
    // Roll uses X and Z axes
    *roll = atan2(x_g, z_g) * 180.0 / M_PI;
}

/**
 * Close ADXL345 file descriptor
 */
void adxl345_close(int file)
{
    if (file >= 0) {
        close(file);
    }
}
