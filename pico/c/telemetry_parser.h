#ifndef TELEMETRY_PARSER_H
#define TELEMETRY_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TRAFFIC 10

// Structure for own aircraft data
typedef struct {
    double lat;
    double lon;
    double alt;
    double pitch;
    double roll;
    double yaw;
} OwnShipData;

// Structure for traffic aircraft
typedef struct {
    char id[8];
    double lat;
    double lon;
    double alt;
} TrafficData;

// Structure for connectivity status
typedef struct {
    bool wifi;
    bool gps;
    bool bluetooth;
} ConnectivityStatus;

// Complete telemetry structure
typedef struct {
    OwnShipData own;
    TrafficData traffic[MAX_TRAFFIC];
    uint8_t traffic_count;
    ConnectivityStatus status;
    bool valid;
} TelemetryData;

// Function to parse JSON telemetry string
bool parse_telemetry(const char* json_str, TelemetryData* telemetry);

#endif // TELEMETRY_PARSER_H
