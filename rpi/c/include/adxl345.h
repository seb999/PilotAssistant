/**
 * ADXL345 Accelerometer Library
 * I2C interface for reading acceleration data and calculating attitude
 */

#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>

// ADXL345 I2C address
#define ADXL345_ADDRESS 0x53

// ADXL345 register addresses
#define ADXL345_POWER_CTL   0x2D
#define ADXL345_DATA_FORMAT 0x31
#define ADXL345_BW_RATE     0x2C
#define ADXL345_DATAX0      0x32

// Scale factor for ±2g range (256 LSB/g)
#define SCALE_FACTOR 256.0

/**
 * Initialize the ADXL345 sensor
 * Returns: file descriptor or -1 on error
 */
int adxl345_init(void);

/**
 * Read raw acceleration data from ADXL345
 * Returns: 0 on success, -1 on error
 */
int adxl345_read_axes(int file, float *x_g, float *y_g, float *z_g);

/**
 * Calculate pitch and roll angles from acceleration data
 * Pitch: rotation around Y-axis (nose up/down)
 * Roll: rotation around X-axis (wing up/down)
 *
 * x_g, y_g, z_g: acceleration in g-forces
 * pitch, roll: output angles in degrees
 */
void adxl345_calculate_attitude(float x_g, float y_g, float z_g, float *pitch, float *roll);

/**
 * Close ADXL345 file descriptor
 */
void adxl345_close(int file);

#endif // ADXL345_H
