/*
 * ADXL345 Accelerometer Debug Program
 * Reads and displays X, Y, Z acceleration values in g-forces
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdbool.h>

// ADXL345 I2C address
#define ADXL345_ADDRESS 0x53

// ADXL345 register addresses
#define ADXL345_POWER_CTL   0x2D
#define ADXL345_DATA_FORMAT 0x31
#define ADXL345_DATAX0      0x32

// Scale factor for ±2g range (256 LSB/g)
#define SCALE_FACTOR 256.0

// Global flag for clean exit
static volatile bool running = true;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

/**
 * Initialize the ADXL345 sensor
 * Returns: file descriptor or -1 on error
 */
int adxl345_init(void) {
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

    // Wake up the ADXL345 (set Measure bit to 1)
    uint8_t config[2] = {ADXL345_POWER_CTL, 0x08};
    if (write(file, config, 2) != 2) {
        perror("Failed to write to POWER_CTL register");
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

    return file;
}

/**
 * Read acceleration data from ADXL345
 */
int adxl345_read_axes(int file, float *x_g, float *y_g, float *z_g) {
    uint8_t data[6];

    // Read 6 bytes starting from DATAX0 register
    // Using i2c_smbus_read_i2c_block_data equivalent
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

int main(void) {
    int file;
    float x, y, z;

    printf("ADXL345 Accelerometer Debug\n");
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize sensor
    file = adxl345_init();
    if (file < 0) {
        fprintf(stderr, "Failed to initialize ADXL345\n");
        return 1;
    }

    printf("ADXL345 initialized successfully\n");
    printf("Reading acceleration data...\n\n");

    // Main loop
    while (running) {
        if (adxl345_read_axes(file, &x, &y, &z) == 0) {
            printf("X: %6.2f g, Y: %6.2f g, Z: %6.2f g\r", x, y, z);
            fflush(stdout);
        }
        usleep(100000); // 100ms delay
    }

    printf("\nStopped.\n");
    close(file);

    return 0;
}
