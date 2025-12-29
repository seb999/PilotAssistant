/**
 * GPS Library Implementation
 * Parses NMEA GPGGA and GPRMC sentences for speed and altitude
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/stat.h>
#include "../include/gps.h"

#define GPIO_BASE_PATH "/sys/class/gpio"

// Helper function prototypes
static int gpio_export(int pin);
static int gpio_set_direction(int pin, const char *direction);
static int gpio_set_value(int pin, int value);
static int parse_nmea_sentence(const char *sentence, GPSData *gps_data);

/**
 * Export GPIO pin
 */
static int gpio_export(int pin) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d", GPIO_BASE_PATH, pin);

    struct stat st;
    if (stat(path, &st) == 0) {
        return 0; // Already exported
    }

    int fd = open(GPIO_BASE_PATH "/export", O_WRONLY);
    if (fd < 0) return -1;

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, len);
    close(fd);
    usleep(100000);
    return 0;
}

/**
 * Set GPIO direction
 */
static int gpio_set_direction(int pin, const char *direction) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/direction", GPIO_BASE_PATH, pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    write(fd, direction, strlen(direction));
    close(fd);
    return 0;
}

/**
 * Set GPIO value
 */
static int gpio_set_value(int pin, int value) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/value", GPIO_BASE_PATH, pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    char buf[2] = {value ? '1' : '0', '\0'};
    write(fd, buf, 1);
    close(fd);
    return 0;
}

/**
 * Initialize GPS module
 */
int gps_init(void) {
    // Enable GPS module via GPIO
    gpio_export(GPS_EN_PIN);
    gpio_set_direction(GPS_EN_PIN, "out");
    gpio_set_value(GPS_EN_PIN, 1);

    // Wait for GPS to boot
    sleep(2);

    // Open serial port
    int fd = open(GPS_PORT, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }

    struct termios options;
    tcgetattr(fd, &options);

    // Set baud rate
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);

    // Configure for raw input
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL);
    options.c_oflag &= ~OPOST;

    // Non-blocking read with 0.1s timeout
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    return fd;
}

/**
 * Parse NMEA sentence
 */
static int parse_nmea_sentence(const char *sentence, GPSData *gps_data) {
    if (!sentence || !gps_data) return 0;

    // Parse GPGGA for altitude and fix quality
    if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        char *tokens[15];
        char *copy = strdup(sentence);
        char *token = strtok(copy, ",");
        int i = 0;

        while (token && i < 15) {
            tokens[i++] = token;
            token = strtok(NULL, ",");
        }

        if (i >= 10) {
            // Field 6: Fix quality (0 = no fix, 1+ = fix)
            gps_data->has_fix = (atoi(tokens[6]) > 0);

            // Field 7: Number of satellites
            gps_data->satellites = atoi(tokens[7]);

            // Field 9: Altitude in meters
            if (strlen(tokens[9]) > 0) {
                gps_data->altitude_meters = atof(tokens[9]);
            }
        }

        free(copy);
        return 1;
    }

    // Parse GPRMC for speed
    if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        char *tokens[13];
        char *copy = strdup(sentence);
        char *token = strtok(copy, ",");
        int i = 0;

        while (token && i < 13) {
            tokens[i++] = token;
            token = strtok(NULL, ",");
        }

        if (i >= 8) {
            // Field 7: Speed in knots
            if (strlen(tokens[7]) > 0) {
                gps_data->speed_knots = atof(tokens[7]);
            }
        }

        free(copy);
        return 1;
    }

    return 0;
}

/**
 * Read GPS data (non-blocking)
 */
int gps_read_data(int fd, GPSData *gps_data) {
    static char buffer[MAX_NMEA_LENGTH];
    static int buf_pos = 0;
    char c;
    int parsed = 0;

    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';

                // Parse the sentence
                if (parse_nmea_sentence(buffer, gps_data)) {
                    parsed = 1;
                }

                buf_pos = 0;
            }
        } else if (c != '\r' && buf_pos < MAX_NMEA_LENGTH - 1) {
            buffer[buf_pos++] = c;
        }
    }

    return parsed;
}

/**
 * Close GPS
 */
void gps_cleanup(int fd) {
    if (fd >= 0) {
        close(fd);
    }
    gpio_set_value(GPS_EN_PIN, 0);
}
