/**
 * ICM20948 9-Axis Motion Sensor Driver
 * Supports: Gyroscope, Accelerometer, Magnetometer
 * Interface: SPI
 */

#ifndef ICM20948_SENSOR_H
#define ICM20948_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Pin definitions for SPI connection
#define ICM20948_CS_PIN   17
#define ICM20948_SCK_PIN  18
#define ICM20948_MOSI_PIN 19
#define ICM20948_MISO_PIN 20

// ICM20948 Register Banks
#define ICM20948_BANK_0 0
#define ICM20948_BANK_1 1
#define ICM20948_BANK_2 2
#define ICM20948_BANK_3 3

// Bank 0 Registers
#define ICM20948_WHO_AM_I       0x00
#define ICM20948_USER_CTRL      0x03
#define ICM20948_PWR_MGMT_1     0x06
#define ICM20948_PWR_MGMT_2     0x07
#define ICM20948_ACCEL_XOUT_H   0x2D
#define ICM20948_GYRO_XOUT_H    0x33
#define ICM20948_TEMP_OUT_H     0x39
#define ICM20948_EXT_SLV_SENS_DATA_00 0x3B
#define ICM20948_REG_BANK_SEL   0x7F

// Bank 2 Registers
#define ICM20948_GYRO_CONFIG_1  0x01
#define ICM20948_ACCEL_CONFIG   0x14

// Bank 3 Registers (I2C Master)
#define ICM20948_I2C_MST_CTRL   0x01
#define ICM20948_I2C_SLV0_ADDR  0x03
#define ICM20948_I2C_SLV0_REG   0x04
#define ICM20948_I2C_SLV0_CTRL  0x05
#define ICM20948_I2C_SLV0_DO    0x06
#define ICM20948_EXT_SLV_SENS_DATA_00 0x3B

// AK09916 Magnetometer Registers (accessed via I2C)
#define AK09916_I2C_ADDR        0x0C
#define AK09916_WHO_AM_I        0x01
#define AK09916_ST1             0x10
#define AK09916_HXL             0x11
#define AK09916_HXH             0x12
#define AK09916_HYL             0x13
#define AK09916_HYH             0x14
#define AK09916_HZL             0x15
#define AK09916_HZH             0x16
#define AK09916_ST2             0x18
#define AK09916_CNTL2           0x31
#define AK09916_CNTL3           0x32

// AK09916 Device ID and Mode
#define AK09916_DEVICE_ID       0x09
#define AK09916_MODE_CONT_100HZ 0x08

// Device ID
#define ICM20948_DEVICE_ID      0xEA

// Full-scale range options
typedef enum {
    GYRO_RANGE_250DPS = 0,   // ±250 degrees/sec
    GYRO_RANGE_500DPS = 1,   // ±500 degrees/sec
    GYRO_RANGE_1000DPS = 2,  // ±1000 degrees/sec
    GYRO_RANGE_2000DPS = 3   // ±2000 degrees/sec
} GyroRange;

typedef enum {
    ACCEL_RANGE_2G = 0,      // ±2g
    ACCEL_RANGE_4G = 1,      // ±4g
    ACCEL_RANGE_8G = 2,      // ±8g
    ACCEL_RANGE_16G = 3      // ±16g
} AccelRange;

// Sensor data structure
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} SensorData;

/**
 * Initialize the ICM20948 sensor
 * Returns: true on success, false on failure
 */
bool icm20948_init(void);

/**
 * Initialize the AK09916 magnetometer
 * Returns: true on success, false on failure
 */
bool icm20948_init_magnetometer(void);

/**
 * Read accelerometer data (raw 16-bit values)
 * Returns: true on success, false on failure
 */
bool icm20948_read_accel(SensorData* data);

/**
 * Read gyroscope data (raw 16-bit values)
 * Returns: true on success, false on failure
 */
bool icm20948_read_gyro(SensorData* data);

/**
 * Read magnetometer data (raw 16-bit values in µT)
 * Returns: true on success, false on failure
 */
bool icm20948_read_mag(SensorData* data);

/**
 * Read temperature sensor (raw 16-bit value)
 * Returns: true on success, false on failure
 */
bool icm20948_read_temp(int16_t* temp);

/**
 * Convert raw accelerometer value to g-force
 * range: Current accelerometer range setting
 */
float icm20948_accel_to_g(int16_t raw_value, AccelRange range);

/**
 * Convert raw gyroscope value to degrees per second
 * range: Current gyroscope range setting
 */
float icm20948_gyro_to_dps(int16_t raw_value, GyroRange range);

/**
 * Convert raw magnetometer value to microTesla
 */
float icm20948_mag_to_ut(int16_t raw_value);

/**
 * Convert raw temperature value to Celsius
 */
float icm20948_temp_to_celsius(int16_t raw_temp);

/**
 * Put sensor into sleep mode
 */
void icm20948_sleep(void);

/**
 * Wake sensor from sleep mode
 */
void icm20948_wake(void);

#endif // ICM20948_SENSOR_H
