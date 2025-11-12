/**
 * Serial Communication Module for Raspberry Pi
 * Handles USB CDC serial communication with Pico2 with auto-reconnect
 */

#ifndef SERIAL_COMM_H
#define SERIAL_COMM_H

#include <stdint.h>
#include <stdbool.h>

// Serial port configuration
#define SERIAL_PORT_PRIMARY "/dev/ttyACM0"
#define SERIAL_PORT_FALLBACK "/dev/ttyUSB0"
#define SERIAL_BAUDRATE 115200
#define SERIAL_READ_BUFFER_SIZE 256

/**
 * Initialize serial communication
 * Attempts to open primary port, falls back to secondary if unavailable
 * Returns file descriptor on success, -1 on failure
 */
int serial_init(void);

/**
 * Close serial connection
 */
void serial_close(int fd);

/**
 * Check if serial port is still valid
 * Returns true if port is open and responsive
 */
bool serial_is_connected(int fd);

/**
 * Attempt to reconnect to serial port
 * Returns new file descriptor on success, -1 on failure
 */
int serial_reconnect(void);

/**
 * Read a line from serial port (blocks until newline or timeout)
 * Returns number of bytes read (excluding newline), or -1 on error
 * Buffer will be null-terminated
 */
int serial_read_line(int fd, char* buffer, size_t buffer_size, int timeout_ms);

/**
 * Read available data from serial port (non-blocking)
 * Returns number of bytes read, 0 if no data available, -1 on error
 */
int serial_read_available(int fd, char* buffer, size_t buffer_size);

/**
 * Write data to serial port
 * Returns number of bytes written, -1 on error
 */
int serial_write(int fd, const char* data, size_t len);

#endif // SERIAL_COMM_H
