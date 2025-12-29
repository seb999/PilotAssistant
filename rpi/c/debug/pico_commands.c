/*
 * Pico2 Command Monitor
 * Listens for commands sent by Pico2 over USB serial (/dev/ttyACM0)
 * Displays button presses, joystick movements, and menu selections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

#define PICO_SERIAL_PORT "/dev/ttyACM0"
#define BAUDRATE B115200
#define MAX_LINE_LENGTH 256

// Global flag for clean exit
static volatile bool running = true;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

/**
 * Initialize serial port connection to Pico2
 * Returns: file descriptor or -1 on error
 */
int pico_serial_init(void) {
    int fd;
    struct termios options;

    // Open serial port
    fd = open(PICO_SERIAL_PORT, O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        perror("Failed to open serial port");
        fprintf(stderr, "Make sure Pico2 is connected to %s\n", PICO_SERIAL_PORT);
        return -1;
    }

    // Get current serial port settings
    if (tcgetattr(fd, &options) < 0) {
        perror("Failed to get serial port attributes");
        close(fd);
        return -1;
    }

    // Set baud rate
    cfsetispeed(&options, BAUDRATE);

    // Configure for raw input (non-canonical mode)
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;  // No parity
    options.c_cflag &= ~CSTOPB;  // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;      // 8 data bits

    // Set read timeout (1 decisecond = 0.1s)
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    // Apply settings
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("Failed to set serial port attributes");
        close(fd);
        return -1;
    }

    // Flush any existing data
    tcflush(fd, TCIFLUSH);

    return fd;
}

/**
 * Parse and display command from Pico2
 */
void parse_command(const char* line) {
    char type[16], param1[64], param2[64];

    // Remove trailing newline/carriage return
    char clean_line[MAX_LINE_LENGTH];
    strncpy(clean_line, line, MAX_LINE_LENGTH - 1);
    clean_line[MAX_LINE_LENGTH - 1] = '\0';

    // Remove trailing whitespace
    int len = strlen(clean_line);
    while (len > 0 && (clean_line[len-1] == '\n' || clean_line[len-1] == '\r' || clean_line[len-1] == ' ')) {
        clean_line[--len] = '\0';
    }

    // Parse BTN:X,ACTION format
    if (sscanf(clean_line, "%[^:]:%[^,],%s", type, param1, param2) == 3) {
        if (strcmp(type, "BTN") == 0) {
            printf("  [BUTTON] Button %s - %s\n", param1, param2);
            return;
        }
    }

    // Parse JOY:DIRECTION or CMD:ACTION format
    if (sscanf(clean_line, "%[^:]:%s", type, param1) == 2) {
        if (strcmp(type, "JOY") == 0) {
            printf("  [JOYSTICK] Direction: %s\n", param1);
            return;
        }
        if (strcmp(type, "CMD") == 0) {
            printf("  >>> [MENU COMMAND] %s <<<\n", param1);
            return;
        }
    }

    // Unknown format - display raw
    printf("  [RAW] %s\n", clean_line);
}

/**
 * Read and process commands from serial port
 */
void process_serial_data(int fd) {
    static char buffer[MAX_LINE_LENGTH];
    static int buffer_pos = 0;
    char read_buffer[64];
    ssize_t bytes_read;

    // Read available data
    bytes_read = read(fd, read_buffer, sizeof(read_buffer) - 1);

    if (bytes_read > 0) {
        read_buffer[bytes_read] = '\0';

        // Process each character
        for (int i = 0; i < bytes_read; i++) {
            char c = read_buffer[i];

            // Line complete on newline
            if (c == '\n' || c == '\r') {
                if (buffer_pos > 0) {
                    buffer[buffer_pos] = '\0';
                    parse_command(buffer);
                    buffer_pos = 0;
                }
            }
            // Add to buffer if space available
            else if (buffer_pos < MAX_LINE_LENGTH - 1) {
                buffer[buffer_pos++] = c;
            }
            // Buffer overflow - discard and reset
            else {
                fprintf(stderr, "Warning: Buffer overflow, discarding data\n");
                buffer_pos = 0;
            }
        }
    }
}

int main(void) {
    int fd;

    printf("===========================================\n");
    printf("  Pico2 Command Monitor\n");
    printf("===========================================\n");
    printf("Port: %s @ 115200 baud\n", PICO_SERIAL_PORT);
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize serial connection
    fd = pico_serial_init();
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize serial port\n");
        fprintf(stderr, "\nTroubleshooting:\n");
        fprintf(stderr, "  1. Check if Pico2 is connected: ls /dev/ttyACM*\n");
        fprintf(stderr, "  2. Check permissions: sudo usermod -a -G dialout $USER\n");
        fprintf(stderr, "  3. Verify Pico2 is running command sender firmware\n");
        return 1;
    }

    printf("Connected successfully!\n");
    printf("Waiting for commands from Pico2...\n");
    printf("-------------------------------------------\n\n");

    // Main loop - read and process commands
    while (running) {
        process_serial_data(fd);
        usleep(10000);  // 10ms delay to prevent CPU spinning
    }

    printf("\n\nStopping...\n");
    close(fd);

    return 0;
}
