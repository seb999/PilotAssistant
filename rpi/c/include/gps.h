/**
 * GPS Library - NMEA sentence parsing for speed and altitude
 */

#ifndef GPS_H
#define GPS_H

#include <stdint.h>
#include <stdbool.h>

#define GPS_PORT "/dev/ttyAMA0"
#define GPS_EN_PIN 17
#define GPS_BAUDRATE 9600
#define MAX_NMEA_LENGTH 256

typedef struct {
    float speed_knots;      // Speed in knots
    float altitude_meters;  // Altitude in meters
    bool has_fix;           // GPS fix available
    int satellites;         // Number of satellites
} GPSData;

/**
 * Initialize GPS module and serial port
 * Returns: file descriptor or -1 on error
 */
int gps_init(void);

/**
 * Read and parse GPS data (non-blocking)
 * Updates gps_data structure
 * Returns: 1 if new data parsed, 0 if no data, -1 on error
 */
int gps_read_data(int fd, GPSData *gps_data);

/**
 * Close GPS and cleanup
 */
void gps_cleanup(int fd);

#endif // GPS_H
