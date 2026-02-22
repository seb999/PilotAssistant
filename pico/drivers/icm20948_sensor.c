/**
 * ICM20948 9-Axis Motion Sensor Driver Implementation
 */

#include "icm20948_sensor.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdio.h>

// SPI instance (using spi0 to avoid conflict with LCD on spi1)
#define ICM20948_SPI spi0
#define ICM20948_SPI_BAUDRATE (7 * 1000 * 1000)  // 7 MHz (maximum supported speed)

// CS timing delays (in microseconds)
// - CS_SETUP_DELAY: delay after CS low before clocking (required for reliable communication)
// - CS_HOLD_DELAY: delay after transaction before CS high (minimum hold time)
// At 7 MHz, start with small delays to improve reliability
#define ICM20948_CS_SETUP_DELAY 1  // µs delay after CS low before clocking
#define ICM20948_CS_HOLD_DELAY  1  // µs delay after transaction before CS high

// Current sensor configuration
static GyroRange current_gyro_range = GYRO_RANGE_500DPS;
static AccelRange current_accel_range = ACCEL_RANGE_4G;
static uint8_t current_bank = 0xFF;  // Track current bank (0xFF = uninitialized)

// Static helper functions
static void select_bank(uint8_t bank);
static uint8_t read_register(uint8_t reg);
static void write_register(uint8_t reg, uint8_t value);
static void read_registers(uint8_t reg, uint8_t* buffer, size_t len);
static void write_ak09916_register(uint8_t reg, uint8_t value);
static uint8_t read_ak09916_register(uint8_t reg);

/**
 * Initialize SPI and configure the ICM20948 sensor
 */
bool icm20948_init(void) {
    printf("Initializing ICM20948 sensor...\n");

    // Initialize SPI
    spi_init(ICM20948_SPI, ICM20948_SPI_BAUDRATE);

    // Set SPI format: 8 bits, SPI Mode 3 (previous stable setting)
    spi_set_format(ICM20948_SPI, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    // Configure SPI pins
    gpio_set_function(ICM20948_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(ICM20948_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(ICM20948_MISO_PIN, GPIO_FUNC_SPI);

    // Configure CS pin as output and set high (inactive)
    gpio_init(ICM20948_CS_PIN);
    gpio_set_dir(ICM20948_CS_PIN, GPIO_OUT);
    gpio_put(ICM20948_CS_PIN, 1);

    // Wait for sensor to power up
    sleep_ms(100);

    // Reset the device FIRST to clear any bad state from previous use
    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x80);  // Reset device
    sleep_ms(100);

    // Wake up the sensor
    write_register(ICM20948_PWR_MGMT_1, 0x01);  // Wake up, auto-select best clock
    sleep_ms(10);

    // NOW verify device ID (after reset)
    select_bank(ICM20948_BANK_0);
    uint8_t who_am_i = read_register(ICM20948_WHO_AM_I);
    printf("WHO_AM_I: 0x%02X (expected 0x%02X)\n", who_am_i, ICM20948_DEVICE_ID);

    if (who_am_i != ICM20948_DEVICE_ID) {
        printf("ERROR: ICM20948 not found!\n");
        return false;
    }

    // Disable I2C interface (we're using SPI only)
    write_register(ICM20948_USER_CTRL, 0x10);   // Disable I2C, enable SPI
    sleep_ms(10);

    // Configure gyroscope (Bank 2)
    select_bank(ICM20948_BANK_2);

    // GYRO_CONFIG_1: ±500 dps, DLPF enabled, 51.2Hz bandwidth
    // Bit [0] = 1: Enable DLPF
    // Bits [2:1] = 01: ±500 dps full scale
    // Bits [5:3] = 011: DLPF bandwidth = 51.2 Hz (good balance for attitude estimation)
    write_register(ICM20948_GYRO_CONFIG_1, 0x1B);  // 0b00011011

    // Configure accelerometer (Bank 2)
    // ACCEL_CONFIG: ±4g, DLPF enabled, 50.4Hz bandwidth
    // Bit [0] = 1: Enable DLPF
    // Bits [2:1] = 01: ±4g full scale
    // Bits [5:3] = 011: DLPF bandwidth = 50.4 Hz (matches gyro for sensor fusion)
    write_register(ICM20948_ACCEL_CONFIG, 0x1B);  // 0b00011011

    // Return to Bank 0 for normal operation
    select_bank(ICM20948_BANK_0);
    

    // Enable accelerometer and gyroscope
    write_register(ICM20948_PWR_MGMT_2, 0x00);  // Enable all sensors

    // Wait for sensors to stabilize
    sleep_ms(100);

    printf("ICM20948 initialized successfully!\n");
    return true;
}

/**
 * Read accelerometer data
 */
bool icm20948_read_accel(SensorData* data) {
    if (data == NULL) return false;

    select_bank(ICM20948_BANK_0);

    uint8_t buffer[6];
    read_registers(ICM20948_ACCEL_XOUT_H, buffer, 6);

    // Combine high and low bytes
    data->x = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->y = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->z = (int16_t)((buffer[4] << 8) | buffer[5]);

    return true;
}

/**
 * Read gyroscope data
 */
bool icm20948_read_gyro(SensorData* data) {
    if (data == NULL) return false;

    select_bank(ICM20948_BANK_0);

    uint8_t buffer[6];
    read_registers(ICM20948_GYRO_XOUT_H, buffer, 6);

    // Combine high and low bytes
    data->x = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->y = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->z = (int16_t)((buffer[4] << 8) | buffer[5]);

    return true;
}

/**
 * Read accelerometer and gyroscope data in single burst (optimized)
 * Reduces SPI overhead by ~30% compared to separate reads
 */
bool icm20948_read_accel_gyro(SensorData* accel, SensorData* gyro) {
    if (accel == NULL || gyro == NULL) return false;

    select_bank(ICM20948_BANK_0);

    // Read 12 bytes: ACCEL_XOUT_H through GYRO_ZOUT_L
    uint8_t buffer[12];
    read_registers(ICM20948_ACCEL_XOUT_H, buffer, 12);

    // Parse accelerometer data (bytes 0-5)
    accel->x = (int16_t)((buffer[0] << 8) | buffer[1]);
    accel->y = (int16_t)((buffer[2] << 8) | buffer[3]);
    accel->z = (int16_t)((buffer[4] << 8) | buffer[5]);

    // Parse gyroscope data (bytes 6-11)
    gyro->x = (int16_t)((buffer[6] << 8) | buffer[7]);
    gyro->y = (int16_t)((buffer[8] << 8) | buffer[9]);
    gyro->z = (int16_t)((buffer[10] << 8) | buffer[11]);

    return true;
}

/**
 * Read temperature sensor
 */
bool icm20948_read_temp(int16_t* temp) {
    if (temp == NULL) return false;

    select_bank(ICM20948_BANK_0);

    uint8_t buffer[2];
    read_registers(ICM20948_TEMP_OUT_H, buffer, 2);

    *temp = (int16_t)((buffer[0] << 8) | buffer[1]);

    return true;
}

/**
 * Convert raw accelerometer value to g-force
 */
float icm20948_accel_to_g(int16_t raw_value, AccelRange range) {
    float sensitivity;

    switch (range) {
        case ACCEL_RANGE_2G:  sensitivity = 16384.0f; break;
        case ACCEL_RANGE_4G:  sensitivity = 8192.0f; break;
        case ACCEL_RANGE_8G:  sensitivity = 4096.0f; break;
        case ACCEL_RANGE_16G: sensitivity = 2048.0f; break;
        default: sensitivity = 8192.0f;
    }

    return (float)raw_value / sensitivity;
}

/**
 * Convert raw gyroscope value to degrees per second
 */
float icm20948_gyro_to_dps(int16_t raw_value, GyroRange range) {
    float sensitivity;

    switch (range) {
        case GYRO_RANGE_250DPS:  sensitivity = 131.0f; break;
        case GYRO_RANGE_500DPS:  sensitivity = 65.5f; break;
        case GYRO_RANGE_1000DPS: sensitivity = 32.8f; break;
        case GYRO_RANGE_2000DPS: sensitivity = 16.4f; break;
        default: sensitivity = 65.5f;
    }

    return (float)raw_value / sensitivity;
}

/**
 * Convert raw temperature to Celsius
 */
float icm20948_temp_to_celsius(int16_t raw_temp) {
    // Formula from ICM20948 datasheet
    return ((float)raw_temp / 333.87f) + 21.0f;
}

/**
 * Put sensor into sleep mode
 */
void icm20948_sleep(void) {
    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x40);  // Set SLEEP bit
}

/**
 * Wake sensor from sleep mode
 */
void icm20948_wake(void) {
    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x01);  // Clear SLEEP bit, auto-select clock
    sleep_ms(10);
}

/**
 * Initialize AK09916 magnetometer via I2C auxiliary interface
 */
bool icm20948_init_magnetometer(void) {
    printf("Initializing AK09916 magnetometer...\n");

    // Enable I2C master mode in Bank 0 while preserving SPI/I2C-disable bits
    select_bank(ICM20948_BANK_0);
    // Keep I2C_IF_DIS (bit4) set for SPI operation and add I2C_MST_EN (bit5)
    uint8_t user_ctrl = read_register(ICM20948_USER_CTRL);
    write_register(ICM20948_USER_CTRL, user_ctrl | 0x30);
    sleep_ms(10);

    // Configure I2C master clock in Bank 3
    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_MST_CTRL, 0x07);  // 400kHz I2C clock
    sleep_ms(10);

    // Verify AK09916 WHO_AM_I
    uint8_t who_am_i = read_ak09916_register(AK09916_WHO_AM_I);
    printf("AK09916 WHO_AM_I: 0x%02X (expected 0x%02X)\n", who_am_i, AK09916_DEVICE_ID);

    if (who_am_i != AK09916_DEVICE_ID) {
        printf("ERROR: AK09916 not found!\n");
        return false;
    }

    // Reset magnetometer
    write_ak09916_register(AK09916_CNTL3, 0x01);
    sleep_ms(100);

    // Set continuous measurement mode 4 (100Hz)
    write_ak09916_register(AK09916_CNTL2, AK09916_MODE_CONT_100HZ);
    sleep_ms(10);

    // Configure automatic magnetometer reading (8 bytes starting from ST1)
    // AK09916 burst read: ST1(1) + HXL..HZH(6) + ST2(1) = 8 bytes total
    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR | 0x80);  // Read mode
    write_register(ICM20948_I2C_SLV0_REG, AK09916_ST1);  // Start at ST1
    write_register(ICM20948_I2C_SLV0_CTRL, 0x88);  // Enable, read 8 bytes

    select_bank(ICM20948_BANK_0);

    printf("AK09916 initialized successfully!\n");
    return true;
}

/**
 * Read magnetometer data from AK09916
 */
bool icm20948_read_mag(SensorData* data) {
    if (data == NULL) return false;

    select_bank(ICM20948_BANK_0);

    // Read 8 bytes from EXT_SLV_SENS_DATA (ST1 + 6 data bytes + ST2)
    // AK09916 burst read: ST1(1) + HXL..HZH(6) + ST2(1) = 8 bytes total
    uint8_t buffer[8];
    read_registers(ICM20948_EXT_SLV_SENS_DATA_00, buffer, 8);

    // Check data ready bit (ST1[0])
    if (!(buffer[0] & 0x01)) {
        return false;  // Data not ready
    }

    // Check overflow bit (ST2[3]) - ST2 is now at buffer[7]
    if (buffer[7] & 0x08) {
        return false;  // Magnetic sensor overflow
    }

    // Combine bytes (LSB first for AK09916)
    // Data bytes are buffer[1..6]
    data->x = (int16_t)((buffer[2] << 8) | buffer[1]);
    data->y = (int16_t)((buffer[4] << 8) | buffer[3]);
    data->z = (int16_t)((buffer[6] << 8) | buffer[5]);

    return true;
}

/**
 * Convert raw magnetometer value to microTesla
 */
float icm20948_mag_to_ut(int16_t raw_value) {
    // AK09916 sensitivity: 0.15 µT/LSB (4912 µT full scale)
    return (float)raw_value * 0.15f;
}

// ============================================================================
// Static Helper Functions
// ============================================================================

/**
 * Select register bank
 */
static void select_bank(uint8_t bank) {
    if (bank == current_bank) return;  // Already on this bank, skip SPI transaction

    gpio_put(ICM20948_CS_PIN, 0);  // CS low
    if (ICM20948_CS_SETUP_DELAY) sleep_us(ICM20948_CS_SETUP_DELAY);

    uint8_t tx[2] = {ICM20948_REG_BANK_SEL, (bank << 4)};
    spi_write_blocking(ICM20948_SPI, tx, 2);

    if (ICM20948_CS_HOLD_DELAY) sleep_us(ICM20948_CS_HOLD_DELAY);
    gpio_put(ICM20948_CS_PIN, 1);  // CS high

    current_bank = bank;  // Update cached bank
}

/**
 * Read a single register
 */
static uint8_t read_register(uint8_t reg) {
    uint8_t value;

    gpio_put(ICM20948_CS_PIN, 0);  // CS low
    if (ICM20948_CS_SETUP_DELAY) sleep_us(ICM20948_CS_SETUP_DELAY);

    // Send register address with read bit set
    uint8_t tx = reg | 0x80;
    spi_write_blocking(ICM20948_SPI, &tx, 1);

    // Read the value
    spi_read_blocking(ICM20948_SPI, 0x00, &value, 1);

    if (ICM20948_CS_HOLD_DELAY) sleep_us(ICM20948_CS_HOLD_DELAY);
    gpio_put(ICM20948_CS_PIN, 1);  // CS high

    return value;
}

/**
 * Write a single register
 */
static void write_register(uint8_t reg, uint8_t value) {
    gpio_put(ICM20948_CS_PIN, 0);  // CS low
    if (ICM20948_CS_SETUP_DELAY) sleep_us(ICM20948_CS_SETUP_DELAY);

    uint8_t tx[2] = {reg & 0x7F, value};  // Clear read bit
    spi_write_blocking(ICM20948_SPI, tx, 2);

    if (ICM20948_CS_HOLD_DELAY) sleep_us(ICM20948_CS_HOLD_DELAY);
    gpio_put(ICM20948_CS_PIN, 1);  // CS high
}

/**
 * Read multiple consecutive registers
 */
static void read_registers(uint8_t reg, uint8_t* buffer, size_t len) {
    gpio_put(ICM20948_CS_PIN, 0);  // CS low
    if (ICM20948_CS_SETUP_DELAY) sleep_us(ICM20948_CS_SETUP_DELAY);

    // Send register address with read bit set
    uint8_t tx = reg | 0x80;
    spi_write_blocking(ICM20948_SPI, &tx, 1);

    // Read the values
    spi_read_blocking(ICM20948_SPI, 0x00, buffer, len);

    if (ICM20948_CS_HOLD_DELAY) sleep_us(ICM20948_CS_HOLD_DELAY);
    gpio_put(ICM20948_CS_PIN, 1);  // CS high
}

/**
 * Write to AK09916 magnetometer register via I2C auxiliary
 */
static void write_ak09916_register(uint8_t reg, uint8_t value) {
    select_bank(ICM20948_BANK_3);

    // Set slave address (write mode)
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR);
    // Set register to write to
    write_register(ICM20948_I2C_SLV0_REG, reg);
    // Set data to write
    write_register(ICM20948_I2C_SLV0_DO, value);
    // Enable write (1 byte)
    write_register(ICM20948_I2C_SLV0_CTRL, 0x81);

    sleep_ms(10);  // Wait for write to complete
}

/**
 * Read from AK09916 magnetometer register via I2C auxiliary
 */
static uint8_t read_ak09916_register(uint8_t reg) {
    select_bank(ICM20948_BANK_3);

    // Set slave address (read mode)
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR | 0x80);
    // Set register to read from
    write_register(ICM20948_I2C_SLV0_REG, reg);
    // Enable read (1 byte)
    write_register(ICM20948_I2C_SLV0_CTRL, 0x81);

    sleep_ms(10);  // Wait for read to complete

    // Read from EXT_SLV_SENS_DATA_00 in Bank 0
    select_bank(ICM20948_BANK_0);
    return read_register(ICM20948_EXT_SLV_SENS_DATA_00);
}
