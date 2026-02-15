/**
 * Serial Communication Module Implementation
 */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include "serial_comm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>

// Configure serial port settings
static int configure_serial(int fd) {
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    // Set baud rate
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // 8N1 mode (8 bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 bits per byte

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;

    // Raw mode (disable canonical mode and echo)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Disable special handling of received bytes
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Disable special output processing
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // Non-blocking read (no timeout for max responsiveness)
    tty.c_cc[VTIME] = 0;  // No timeout
    tty.c_cc[VMIN] = 0;   // Return immediately with available data

    // Apply settings
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    // Flush any existing data
    tcflush(fd, TCIOFLUSH);

    return 0;
}

// Try to open a specific serial port
static int try_open_port(const char* port_path) {
    // Check if device exists
    struct stat st;
    if (stat(port_path, &st) != 0) {
        return -1;  // Device doesn't exist
    }

    int fd = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    if (configure_serial(fd) != 0) {
        close(fd);
        return -1;
    }

    printf("Opened serial port: %s\n", port_path);
    return fd;
}

int serial_init(void) {
    int fd;

    // Try primary port first
    fd = try_open_port(SERIAL_PORT_PRIMARY);
    if (fd >= 0) {
        return fd;
    }

    // Try fallback port
    fd = try_open_port(SERIAL_PORT_FALLBACK);
    if (fd >= 0) {
        return fd;
    }

    fprintf(stderr, "Failed to open any serial port\n");
    return -1;
}

void serial_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

bool serial_is_connected(int fd) {
    if (fd < 0) {
        return false;
    }

    // Check if file descriptor is still valid
    int error = 0;
    socklen_t len = sizeof(error);

    // Use fcntl to check if fd is valid
    if (fcntl(fd, F_GETFL) == -1) {
        return false;
    }

    return true;
}

int serial_reconnect(void) {
    printf("Attempting to reconnect to serial port...\n");

    // Wait a bit before trying to reconnect
    sleep(1);

    return serial_init();
}

int serial_read_line(int fd, char* buffer, size_t buffer_size, int timeout_ms) {
    if (fd < 0 || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    size_t pos = 0;
    struct timeval tv;
    fd_set readfds;

    // Calculate timeout
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    while (pos < buffer_size - 1) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0) {
            if (errno == EINTR) continue;  // Interrupted, try again
            return -1;  // Error
        }

        if (ret == 0) {
            // Timeout - if we have data, return it
            if (pos > 0) {
                buffer[pos] = '\0';
                return pos;
            }
            return 0;  // No data
        }

        // Data available
        char ch;
        ssize_t n = read(fd, &ch, 1);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;  // Error
        }

        if (n == 0) {
            // EOF or disconnect
            return -1;
        }

        // Check for newline
        if (ch == '\n' || ch == '\r') {
            buffer[pos] = '\0';
            // Skip any additional newline characters
            if (ch == '\r') {
                // Peek for \n
                char peek;
                if (read(fd, &peek, 1) == 1 && peek != '\n') {
                    // Put it back somehow, or just ignore
                }
            }
            return pos;
        }

        buffer[pos++] = ch;
    }

    buffer[pos] = '\0';
    return pos;
}

int serial_read_available(int fd, char* buffer, size_t buffer_size) {
    if (fd < 0 || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    // Check if data is available
    int bytes_available;
    if (ioctl(fd, FIONREAD, &bytes_available) < 0) {
        return -1;
    }

    if (bytes_available == 0) {
        return 0;
    }

    // Read available data
    size_t to_read = bytes_available < (int)buffer_size ? bytes_available : buffer_size;
    ssize_t n = read(fd, buffer, to_read);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }

    return n;
}

int serial_write(int fd, const char* data, size_t len) {
    if (fd < 0 || data == NULL || len == 0) {
        return -1;
    }

    ssize_t written = write(fd, data, len);
    return written;
}
