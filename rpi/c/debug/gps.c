/*
 * GPS Debug Program
 * Reads and displays raw NMEA sentences from GPS module
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

// GPS Configuration (matching Python config.py)
#define GPS_EN_PIN      17
#define GPS_PORT        "/dev/ttyAMA0"
#define GPS_BAUDRATE    B9600
#define GPS_TIMEOUT     1
#define MAX_LINE_LENGTH 256

// GPIO paths
#define GPIO_BASE_PATH  "/sys/class/gpio"

// Global flag for clean exit
static volatile bool running = true;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

/**
 * Export GPIO pin
 */
int gpio_export(int pin) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d", GPIO_BASE_PATH, pin);

    // Check if already exported
    struct stat st;
    if (stat(path, &st) == 0) {
        return 0; // Already exported
    }

    int fd = open(GPIO_BASE_PATH "/export", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open GPIO export");
        return -1;
    }

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", pin);
    if (write(fd, buf, len) < 0) {
        if (errno != EBUSY) {  // EBUSY means already exported
            perror("Failed to export GPIO");
            close(fd);
            return -1;
        }
    }

    close(fd);
    usleep(100000); // Wait for sysfs to create files
    return 0;
}

/**
 * Set GPIO direction
 */
int gpio_set_direction(int pin, const char *direction) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/direction", GPIO_BASE_PATH, pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open GPIO direction");
        return -1;
    }

    if (write(fd, direction, strlen(direction)) < 0) {
        perror("Failed to set GPIO direction");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
 * Set GPIO value
 */
int gpio_set_value(int pin, int value) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/value", GPIO_BASE_PATH, pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open GPIO value");
        return -1;
    }

    char buf[2] = {value ? '1' : '0', '\0'};
    if (write(fd, buf, 1) < 0) {
        perror("Failed to set GPIO value");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
 * Unexport GPIO pin
 */
void gpio_unexport(int pin) {
    int fd = open(GPIO_BASE_PATH "/unexport", O_WRONLY);
    if (fd < 0) {
        return;
    }

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, len);
    close(fd);
}

/**
 * Initialize GPS enable pin
 */
int gps_enable_pin_init(void) {
    printf("Initializing GPS enable pin (GPIO%d)...\n", GPS_EN_PIN);

    if (gpio_export(GPS_EN_PIN) < 0) {
        fprintf(stderr, "⚠ Error exporting GPS enable pin\n");
        return -1;
    }

    if (gpio_set_direction(GPS_EN_PIN, "out") < 0) {
        fprintf(stderr, "⚠ Error setting GPS pin direction\n");
        return -1;
    }

    if (gpio_set_value(GPS_EN_PIN, 1) < 0) {
        fprintf(stderr, "⚠ Error setting GPS pin HIGH\n");
        return -1;
    }

    printf("✓ GPS EN pin set to HIGH (GPIO%d)\n", GPS_EN_PIN);
    printf("Waiting 3 seconds for GPS module to boot...\n");
    sleep(3);

    return 0;
}

/**
 * Initialize serial port for GPS
 */
int gps_serial_init(const char *port) {
    int fd;
    struct termios options;

    // Open serial port
    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Failed to open GPS serial port");
        return -1;
    }

    // Get current options
    if (tcgetattr(fd, &options) < 0) {
        perror("Failed to get serial attributes");
        close(fd);
        return -1;
    }

    // Set baud rate
    cfsetispeed(&options, GPS_BAUDRATE);
    cfsetospeed(&options, GPS_BAUDRATE);

    // Configure for raw input (no processing)
    options.c_cflag |= (CLOCAL | CREAD);    // Enable receiver, ignore modem control
    options.c_cflag &= ~PARENB;             // No parity
    options.c_cflag &= ~CSTOPB;             // 1 stop bit
    options.c_cflag &= ~CSIZE;              // Clear character size bits
    options.c_cflag |= CS8;                 // 8 data bits
    options.c_cflag &= ~CRTSCTS;            // No hardware flow control

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_iflag &= ~(IXON | IXOFF | IXANY);         // No software flow control
    options.c_iflag &= ~(INLCR | ICRNL);                // Don't translate CR/LF

    options.c_oflag &= ~OPOST;              // Raw output

    // Set timeout
    options.c_cc[VMIN] = 0;                 // Non-blocking read
    options.c_cc[VTIME] = GPS_TIMEOUT * 10; // Timeout in deciseconds

    // Apply options
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("Failed to set serial attributes");
        close(fd);
        return -1;
    }

    // Flush any existing data
    tcflush(fd, TCIOFLUSH);

    return fd;
}

/**
 * Read a line from serial port
 */
int read_line(int fd, char *buffer, size_t size) {
    size_t i = 0;
    char c;

    while (i < size - 1) {
        ssize_t n = read(fd, &c, 1);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        } else if (n == 0) {
            // Timeout
            if (i > 0) {
                break;
            }
            return 0;
        }

        if (c == '\n') {
            buffer[i] = '\0';
            return i;
        } else if (c != '\r') {
            buffer[i++] = c;
        }
    }

    buffer[i] = '\0';
    return i;
}

int main(void) {
    int serial_fd;
    char line[MAX_LINE_LENGTH];
    int line_count = 0;
    bool data_detected = false;
    time_t start_time;

    printf("=== GPS Detection and Raw Data Display ===\n");
    printf("GPS Port: %s\n", GPS_PORT);
    printf("GPS Baudrate: 9600\n");
    printf("GPS Timeout: %d second(s)\n", GPS_TIMEOUT);
    printf("GPS Enable Pin: %d\n\n", GPS_EN_PIN);

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize GPS enable pin
    if (gps_enable_pin_init() < 0) {
        fprintf(stderr, "Warning: Could not initialize GPS enable pin\n");
    }

    // Open serial port
    serial_fd = gps_serial_init(GPS_PORT);
    if (serial_fd < 0) {
        fprintf(stderr, "✗ Error opening serial port %s\n", GPS_PORT);
        fprintf(stderr, "Check if GPS module is connected and port is correct\n");
        gpio_unexport(GPS_EN_PIN);
        return 1;
    }

    printf("✓ Serial port %s opened successfully\n", GPS_PORT);
    printf("Listening for GPS data... (Press Ctrl+C to stop)\n");
    printf("==================================================\n");

    start_time = time(NULL);

    // Main loop
    while (running) {
        int len = read_line(serial_fd, line, sizeof(line));

        if (len > 0) {
            if (!data_detected) {
                printf("✓ GPS data detected!\n");
                data_detected = true;
            }

            line_count++;

            // Get timestamp
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char timestamp[16];
            strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

            printf("[%s] Line %d: %s\n", timestamp, line_count, line);
        } else if (len == 0) {
            // Timeout - check if we should warn
            time_t elapsed = time(NULL) - start_time;
            if (elapsed > 10 && !data_detected) {
                printf("⚠ No GPS data received after %ld seconds - check connections\n", elapsed);
                start_time = time(NULL); // Reset timer
            }
        }
    }

    // Cleanup
    printf("\n\n=== Session Summary ===\n");
    printf("Total lines received: %d\n", line_count);
    if (data_detected) {
        printf("✓ GPS module detected and communicating\n");
    } else {
        printf("✗ No GPS data detected - check hardware connections\n");
    }

    close(serial_fd);
    gpio_set_value(GPS_EN_PIN, 0);
    gpio_unexport(GPS_EN_PIN);
    printf("Serial port closed and GPIO cleaned up.\n");

    return 0;
}
