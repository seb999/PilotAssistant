/**
 * ICM20948 9-Axis Motion Sensor Driver — SPI0 interface
 */

#include "icm20948_sensor.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define ICM20948_SPI      spi0
#define ICM20948_BAUDRATE (7 * 1000 * 1000)

static GyroRange  current_gyro_range  = GYRO_RANGE_500DPS;
static AccelRange current_accel_range = ACCEL_RANGE_4G;
static uint8_t    current_bank = 0xFF;

static void    select_bank(uint8_t bank);
static uint8_t read_register(uint8_t reg);
static void    write_register(uint8_t reg, uint8_t value);
static void    read_registers(uint8_t reg, uint8_t *buffer, size_t len);
static void    write_ak09916_register(uint8_t reg, uint8_t value);
static uint8_t read_ak09916_register(uint8_t reg);

// ── SPI transport ─────────────────────────────────────────────────────────────

static void select_bank(uint8_t bank) {
    if (bank == current_bank) return;
    gpio_put(ICM20948_CS_PIN, 0);
    uint8_t tx[2] = {ICM20948_REG_BANK_SEL, (uint8_t)(bank << 4)};
    spi_write_blocking(ICM20948_SPI, tx, 2);
    gpio_put(ICM20948_CS_PIN, 1);
    current_bank = bank;
}

static uint8_t read_register(uint8_t reg) {
    uint8_t val;
    gpio_put(ICM20948_CS_PIN, 0);
    uint8_t tx = reg | 0x80;
    spi_write_blocking(ICM20948_SPI, &tx, 1);
    spi_read_blocking(ICM20948_SPI, 0x00, &val, 1);
    gpio_put(ICM20948_CS_PIN, 1);
    return val;
}

static void write_register(uint8_t reg, uint8_t value) {
    gpio_put(ICM20948_CS_PIN, 0);
    uint8_t tx[2] = {reg & 0x7F, value};
    spi_write_blocking(ICM20948_SPI, tx, 2);
    gpio_put(ICM20948_CS_PIN, 1);
}

static void read_registers(uint8_t reg, uint8_t *buffer, size_t len) {
    gpio_put(ICM20948_CS_PIN, 0);
    uint8_t tx = reg | 0x80;
    spi_write_blocking(ICM20948_SPI, &tx, 1);
    spi_read_blocking(ICM20948_SPI, 0x00, buffer, len);
    gpio_put(ICM20948_CS_PIN, 1);
}

// ── Init ──────────────────────────────────────────────────────────────────────

bool icm20948_init(void) {
    printf("Initializing ICM20948 (SPI0)...\n");

    spi_init(ICM20948_SPI, ICM20948_BAUDRATE);
    spi_set_format(ICM20948_SPI, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(ICM20948_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(ICM20948_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(ICM20948_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(ICM20948_CS_PIN);
    gpio_set_dir(ICM20948_CS_PIN, GPIO_OUT);
    gpio_put(ICM20948_CS_PIN, 1);

    sleep_ms(100);

    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x80);  // reset
    sleep_ms(100);
    write_register(ICM20948_PWR_MGMT_1, 0x01);  // wake, auto-clock
    sleep_ms(10);

    uint8_t who = read_register(ICM20948_WHO_AM_I);
    printf("WHO_AM_I: 0x%02X (expected 0x%02X)\n", who, ICM20948_DEVICE_ID);
    if (who != ICM20948_DEVICE_ID) {
        printf("ERROR: ICM20948 not found!\n");
        return false;
    }

    write_register(ICM20948_USER_CTRL, 0x10);  // disable I2C, SPI only

    // Gyro: ±500 dps, DLPF 51.2 Hz (Bank 2)
    select_bank(ICM20948_BANK_2);
    write_register(ICM20948_GYRO_CONFIG_1, 0x1B);

    // Accel: ±4g, DLPF 50.4 Hz (Bank 2)
    write_register(ICM20948_ACCEL_CONFIG, 0x1B);

    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_2, 0x00);  // enable all sensors
    sleep_ms(100);

    printf("ICM20948 initialized successfully!\n");
    return true;
}

// ── Sensor reads ──────────────────────────────────────────────────────────────

bool icm20948_read_accel(SensorData *data) {
    if (!data) return false;
    select_bank(ICM20948_BANK_0);
    uint8_t buf[6];
    read_registers(ICM20948_ACCEL_XOUT_H, buf, 6);
    data->x = (int16_t)((buf[0] << 8) | buf[1]);
    data->y = (int16_t)((buf[2] << 8) | buf[3]);
    data->z = (int16_t)((buf[4] << 8) | buf[5]);
    return true;
}

bool icm20948_read_gyro(SensorData *data) {
    if (!data) return false;
    select_bank(ICM20948_BANK_0);
    uint8_t buf[6];
    read_registers(ICM20948_GYRO_XOUT_H, buf, 6);
    data->x = (int16_t)((buf[0] << 8) | buf[1]);
    data->y = (int16_t)((buf[2] << 8) | buf[3]);
    data->z = (int16_t)((buf[4] << 8) | buf[5]);
    return true;
}

bool icm20948_read_accel_gyro(SensorData *accel, SensorData *gyro) {
    if (!accel || !gyro) return false;
    select_bank(ICM20948_BANK_0);
    uint8_t buf[12];
    read_registers(ICM20948_ACCEL_XOUT_H, buf, 12);
    accel->x = (int16_t)((buf[0]  << 8) | buf[1]);
    accel->y = (int16_t)((buf[2]  << 8) | buf[3]);
    accel->z = (int16_t)((buf[4]  << 8) | buf[5]);
    gyro->x  = (int16_t)((buf[6]  << 8) | buf[7]);
    gyro->y  = (int16_t)((buf[8]  << 8) | buf[9]);
    gyro->z  = (int16_t)((buf[10] << 8) | buf[11]);
    return true;
}

bool icm20948_read_temp(int16_t *temp) {
    if (!temp) return false;
    select_bank(ICM20948_BANK_0);
    uint8_t buf[2];
    read_registers(ICM20948_TEMP_OUT_H, buf, 2);
    *temp = (int16_t)((buf[0] << 8) | buf[1]);
    return true;
}

// ── Unit conversions ──────────────────────────────────────────────────────────

float icm20948_accel_to_g(int16_t raw, AccelRange range) {
    float s;
    switch (range) {
        case ACCEL_RANGE_2G:  s = 16384.0f; break;
        case ACCEL_RANGE_4G:  s =  8192.0f; break;
        case ACCEL_RANGE_8G:  s =  4096.0f; break;
        case ACCEL_RANGE_16G: s =  2048.0f; break;
        default:              s =  8192.0f;
    }
    return (float)raw / s;
}

float icm20948_gyro_to_dps(int16_t raw, GyroRange range) {
    float s;
    switch (range) {
        case GYRO_RANGE_250DPS:  s = 131.0f; break;
        case GYRO_RANGE_500DPS:  s =  65.5f; break;
        case GYRO_RANGE_1000DPS: s =  32.8f; break;
        case GYRO_RANGE_2000DPS: s =  16.4f; break;
        default:                 s =  65.5f;
    }
    return (float)raw / s;
}

float icm20948_temp_to_celsius(int16_t raw) {
    return ((float)raw / 333.87f) + 21.0f;
}

// ── Power management ──────────────────────────────────────────────────────────

void icm20948_sleep(void) {
    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x40);
}

void icm20948_wake(void) {
    select_bank(ICM20948_BANK_0);
    write_register(ICM20948_PWR_MGMT_1, 0x01);
    sleep_ms(10);
}

// ── Magnetometer (AK09916 via SPI aux I2C master) ─────────────────────────────

bool icm20948_init_magnetometer(void) {
    printf("Initializing AK09916 magnetometer...\n");

    select_bank(ICM20948_BANK_0);
    uint8_t uc = read_register(ICM20948_USER_CTRL);
    write_register(ICM20948_USER_CTRL, uc | 0x30);  // I2C_MST_EN + I2C_IF_DIS
    sleep_ms(10);

    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_MST_CTRL, 0x07);
    sleep_ms(10);

    uint8_t who = read_ak09916_register(AK09916_WHO_AM_I);
    printf("AK09916 WHO_AM_I: 0x%02X (expected 0x%02X)\n", who, AK09916_DEVICE_ID);
    if (who != AK09916_DEVICE_ID) {
        printf("ERROR: AK09916 not found!\n");
        return false;
    }

    write_ak09916_register(AK09916_CNTL3, 0x01);
    sleep_ms(100);
    write_ak09916_register(AK09916_CNTL2, AK09916_MODE_CONT_100HZ);
    sleep_ms(10);

    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR | 0x80);
    write_register(ICM20948_I2C_SLV0_REG,  AK09916_ST1);
    write_register(ICM20948_I2C_SLV0_CTRL, 0x88);

    select_bank(ICM20948_BANK_0);
    printf("AK09916 initialized successfully!\n");
    return true;
}

bool icm20948_read_mag(SensorData *data) {
    if (!data) return false;
    select_bank(ICM20948_BANK_0);
    uint8_t buf[8];
    read_registers(ICM20948_EXT_SLV_SENS_DATA_00, buf, 8);
    if (!(buf[0] & 0x01)) return false;
    if (  buf[7] & 0x08)  return false;
    data->x = (int16_t)((buf[2] << 8) | buf[1]);
    data->y = (int16_t)((buf[4] << 8) | buf[3]);
    data->z = (int16_t)((buf[6] << 8) | buf[5]);
    return true;
}

float icm20948_mag_to_ut(int16_t raw) {
    return (float)raw * 0.15f;
}

static void write_ak09916_register(uint8_t reg, uint8_t value) {
    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR);
    write_register(ICM20948_I2C_SLV0_REG,  reg);
    write_register(ICM20948_I2C_SLV0_DO,   value);
    write_register(ICM20948_I2C_SLV0_CTRL, 0x81);
    sleep_ms(10);
}

static uint8_t read_ak09916_register(uint8_t reg) {
    select_bank(ICM20948_BANK_3);
    write_register(ICM20948_I2C_SLV0_ADDR, AK09916_I2C_ADDR | 0x80);
    write_register(ICM20948_I2C_SLV0_REG,  reg);
    write_register(ICM20948_I2C_SLV0_CTRL, 0x81);
    sleep_ms(10);
    select_bank(ICM20948_BANK_0);
    return read_register(ICM20948_EXT_SLV_SENS_DATA_00);
}
