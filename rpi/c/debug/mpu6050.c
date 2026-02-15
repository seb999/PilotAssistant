/*
 * MPU-6050 I2C Test Program
 * Tests communication with MPU-6050 6-axis IMU sensor
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <time.h>

// MPU-6050 I2C address
#define MPU6050_ADDR 0x68  // Default address (AD0 pin low)
// Alternate address: 0x69 if AD0 pin is high

// MPU-6050 Register addresses
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_WHO_AM_I     0x75

// Data registers
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H   0x41
#define MPU6050_REG_GYRO_XOUT_H  0x43

// Gyro full scale ranges
#define MPU6050_GYRO_FS_250      0x00  // ±250 °/s
#define MPU6050_GYRO_FS_500      0x08  // ±500 °/s
#define MPU6050_GYRO_FS_1000     0x10  // ±1000 °/s
#define MPU6050_GYRO_FS_2000     0x18  // ±2000 °/s

// Accel full scale ranges
#define MPU6050_ACCEL_FS_2       0x00  // ±2g
#define MPU6050_ACCEL_FS_4       0x08  // ±4g
#define MPU6050_ACCEL_FS_8       0x10  // ±8g
#define MPU6050_ACCEL_FS_16      0x18  // ±16g

// I2C device
static int i2c_fd = -1;

// Write a byte to MPU-6050 register
int mpu6050_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(i2c_fd, buf, 2) != 2) {
        perror("Failed to write to MPU-6050");
        return -1;
    }
    return 0;
}

// Read a byte from MPU-6050 register
int mpu6050_read_byte(uint8_t reg, uint8_t *value) {
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write register address");
        return -1;
    }
    if (read(i2c_fd, value, 1) != 1) {
        perror("Failed to read from MPU-6050");
        return -1;
    }
    return 0;
}

// Read multiple bytes from MPU-6050
int mpu6050_read_bytes(uint8_t reg, uint8_t *buffer, int length) {
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write register address");
        return -1;
    }
    if (read(i2c_fd, buffer, length) != length) {
        perror("Failed to read from MPU-6050");
        return -1;
    }
    return 0;
}

// Initialize MPU-6050
int mpu6050_init(void) {
    // Open I2C bus
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }

    // Set I2C slave address
    if (ioctl(i2c_fd, I2C_SLAVE, MPU6050_ADDR) < 0) {
        perror("Failed to set I2C slave address");
        close(i2c_fd);
        return -1;
    }

    // Check WHO_AM_I register (should return 0x68)
    uint8_t who_am_i;
    if (mpu6050_read_byte(MPU6050_REG_WHO_AM_I, &who_am_i) < 0) {
        printf("Failed to read WHO_AM_I register\n");
        close(i2c_fd);
        return -1;
    }
    printf("WHO_AM_I: 0x%02X (expected 0x68)\n", who_am_i);
    if (who_am_i != 0x68) {
        printf("WARNING: Unexpected WHO_AM_I value!\n");
    }

    // Wake up MPU-6050 (reset PWR_MGMT_1 register)
    if (mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00) < 0) {
        printf("Failed to wake up MPU-6050\n");
        close(i2c_fd);
        return -1;
    }
    usleep(100000); // Wait 100ms for sensor to stabilize

    // Set sample rate divider (sample rate = 8kHz / (1 + divider))
    // Divider = 7 gives ~1kHz sample rate
    if (mpu6050_write_byte(MPU6050_REG_SMPLRT_DIV, 0x07) < 0) {
        printf("Failed to set sample rate\n");
        return -1;
    }

    // Set DLPF (Digital Low Pass Filter)
    // CONFIG = 0x06 gives 5Hz bandwidth (good for smooth data)
    if (mpu6050_write_byte(MPU6050_REG_CONFIG, 0x06) < 0) {
        printf("Failed to set config\n");
        return -1;
    }

    // Set gyro range to ±250°/s
    if (mpu6050_write_byte(MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_FS_250) < 0) {
        printf("Failed to set gyro config\n");
        return -1;
    }

    // Set accelerometer range to ±2g
    if (mpu6050_write_byte(MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_2) < 0) {
        printf("Failed to set accel config\n");
        return -1;
    }

    printf("MPU-6050 initialized successfully\n");
    return 0;
}

// Read raw accelerometer data
int mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t data[6];
    if (mpu6050_read_bytes(MPU6050_REG_ACCEL_XOUT_H, data, 6) < 0) {
        return -1;
    }
    *ax = (int16_t)((data[0] << 8) | data[1]);
    *ay = (int16_t)((data[2] << 8) | data[3]);
    *az = (int16_t)((data[4] << 8) | data[5]);
    return 0;
}

// Read raw gyroscope data
int mpu6050_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t data[6];
    if (mpu6050_read_bytes(MPU6050_REG_GYRO_XOUT_H, data, 6) < 0) {
        return -1;
    }
    *gx = (int16_t)((data[0] << 8) | data[1]);
    *gy = (int16_t)((data[2] << 8) | data[3]);
    *gz = (int16_t)((data[4] << 8) | data[5]);
    return 0;
}

// Read temperature
int mpu6050_read_temp(float *temp) {
    uint8_t data[2];
    if (mpu6050_read_bytes(MPU6050_REG_TEMP_OUT_H, data, 2) < 0) {
        return -1;
    }
    int16_t raw_temp = (int16_t)((data[0] << 8) | data[1]);
    *temp = (raw_temp / 340.0f) + 36.53f;
    return 0;
}

// Convert raw accel to g
void accel_to_g(int16_t raw, float *g) {
    // For ±2g range: LSB sensitivity = 16384 LSB/g
    *g = raw / 16384.0f;
}

// Convert raw gyro to degrees/sec
void gyro_to_dps(int16_t raw, float *dps) {
    // For ±250°/s range: LSB sensitivity = 131 LSB/(°/s)
    *dps = raw / 131.0f;
}

// Calculate pitch and roll from accelerometer
void calculate_angles(float ax, float ay, float az, float *pitch, float *roll) {
    *pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
    *roll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0f / M_PI;
}

int main(int argc, char *argv[]) {
    printf("=== MPU-6050 I2C Test ===\n\n");

    // Initialize MPU-6050
    if (mpu6050_init() < 0) {
        printf("Failed to initialize MPU-6050\n");
        return 1;
    }

    printf("\nStarting continuous read (Ctrl+C to exit)...\n\n");
    printf("%-10s %-10s %-10s | %-10s %-10s %-10s | %-8s %-8s | %-6s\n",
           "Accel X", "Accel Y", "Accel Z",
           "Gyro X", "Gyro Y", "Gyro Z",
           "Pitch", "Roll", "Temp");
    printf("---------------------------------------------------------------"
           "--------------------------------------\n");

    while (1) {
        int16_t ax_raw, ay_raw, az_raw;
        int16_t gx_raw, gy_raw, gz_raw;
        float temp;

        // Read accelerometer
        if (mpu6050_read_accel(&ax_raw, &ay_raw, &az_raw) < 0) {
            printf("Failed to read accelerometer\n");
            continue;
        }

        // Read gyroscope
        if (mpu6050_read_gyro(&gx_raw, &gy_raw, &gz_raw) < 0) {
            printf("Failed to read gyroscope\n");
            continue;
        }

        // Read temperature
        if (mpu6050_read_temp(&temp) < 0) {
            printf("Failed to read temperature\n");
            continue;
        }

        // Convert to physical units
        float ax, ay, az, gx, gy, gz;
        accel_to_g(ax_raw, &ax);
        accel_to_g(ay_raw, &ay);
        accel_to_g(az_raw, &az);
        gyro_to_dps(gx_raw, &gx);
        gyro_to_dps(gy_raw, &gy);
        gyro_to_dps(gz_raw, &gz);

        // Calculate angles
        float pitch, roll;
        calculate_angles(ax, ay, az, &pitch, &roll);

        // Display data
        printf("%+9.3fg %+9.3fg %+9.3fg | "
               "%+9.2f° %+9.2f° %+9.2f° | "
               "%+7.1f° %+7.1f° | %5.1f°C\n",
               ax, ay, az,
               gx, gy, gz,
               pitch, roll,
               temp);

        usleep(100000); // 100ms delay (10Hz update rate)
    }

    close(i2c_fd);
    return 0;
}
