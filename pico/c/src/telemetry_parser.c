#include "telemetry_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Simple JSON parser for telemetry data
// This is a lightweight parser specific to our telemetry format
// Format: {"own":{"lat":37.775,"lon":-122.419,"alt":5000,"pitch":1.2,"roll":3.4,"yaw":45.6},"traffic":[{"id":"T1","lat":37.78,"lon":-122.42,"alt":5200}]}

static char* find_key(const char* json, const char* key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    return strstr(json, search);
}

static double extract_double(const char* json, const char* key) {
    char* pos = find_key(json, key);
    if (!pos) return 0.0;

    pos = strchr(pos, ':');
    if (!pos) return 0.0;

    return atof(pos + 1);
}

static void extract_string(const char* json, const char* key, char* output, size_t max_len) {
    char* pos = find_key(json, key);
    if (!pos) {
        output[0] = '\0';
        return;
    }

    pos = strchr(pos, '"');
    if (!pos) {
        output[0] = '\0';
        return;
    }
    pos++; // Skip opening quote

    pos = strchr(pos, '"');
    if (!pos) {
        output[0] = '\0';
        return;
    }
    pos++; // Skip second quote

    pos = strchr(pos, '"');
    if (!pos) {
        output[0] = '\0';
        return;
    }
    pos++; // Now at start of value

    size_t i = 0;
    while (i < max_len - 1 && *pos != '"' && *pos != '\0') {
        output[i++] = *pos++;
    }
    output[i] = '\0';
}

static bool extract_bool(const char* json, const char* key) {
    char* pos = find_key(json, key);
    if (!pos) return false;

    pos = strchr(pos, ':');
    if (!pos) return false;

    // Skip whitespace
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;

    // Check for "true"
    if (strncmp(pos, "true", 4) == 0) {
        return true;
    }

    return false;
}

bool parse_telemetry(const char* json_str, TelemetryData* telemetry) {
    if (!json_str || !telemetry) {
        return false;
    }

    memset(telemetry, 0, sizeof(TelemetryData));

    // Find "own" section
    char* own_section = strstr(json_str, "\"own\"");
    if (!own_section) {
        telemetry->valid = false;
        return false;
    }

    // Parse own aircraft data
    telemetry->own.lat = extract_double(own_section, "lat");
    telemetry->own.lon = extract_double(own_section, "lon");
    telemetry->own.alt = extract_double(own_section, "alt");
    telemetry->own.pitch = extract_double(own_section, "pitch");
    telemetry->own.roll = extract_double(own_section, "roll");
    telemetry->own.yaw = extract_double(own_section, "yaw");

    // Find "traffic" array
    char* traffic_section = strstr(json_str, "\"traffic\"");
    if (traffic_section) {
        char* traffic_start = strchr(traffic_section, '[');
        if (traffic_start) {
            char* current = traffic_start + 1;
            telemetry->traffic_count = 0;

            // Parse each traffic object
            while (telemetry->traffic_count < MAX_TRAFFIC) {
                char* obj_start = strchr(current, '{');
                if (!obj_start) break;

                char* obj_end = strchr(obj_start, '}');
                if (!obj_end) break;

                // Extract traffic data
                TrafficData* t = &telemetry->traffic[telemetry->traffic_count];
                extract_string(obj_start, "id", t->id, sizeof(t->id));
                t->lat = extract_double(obj_start, "lat");
                t->lon = extract_double(obj_start, "lon");
                t->alt = extract_double(obj_start, "alt");

                telemetry->traffic_count++;
                current = obj_end + 1;

                // Check if we've reached the end of the array
                char* next_comma = strchr(current, ',');
                char* array_end = strchr(current, ']');
                if (!array_end || (next_comma && next_comma > array_end)) {
                    break;
                }
                current = next_comma ? next_comma + 1 : array_end;
            }
        }
    }

    // Find "status" section
    char* status_section = strstr(json_str, "\"status\"");
    if (status_section) {
        telemetry->status.wifi = extract_bool(status_section, "wifi");
        telemetry->status.gps = extract_bool(status_section, "gps");
        telemetry->status.bluetooth = extract_bool(status_section, "bluetooth");
    } else {
        // Default to disconnected if status not present
        telemetry->status.wifi = false;
        telemetry->status.gps = false;
        telemetry->status.bluetooth = false;
    }

    // Find "warnings" section
    char* warnings_section = strstr(json_str, "\"warnings\"");
    if (warnings_section) {
        telemetry->warnings.bank_warning = extract_bool(warnings_section, "bank");
        telemetry->warnings.pitch_warning = extract_bool(warnings_section, "pitch");
    } else {
        // Default to no warnings if section not present
        telemetry->warnings.bank_warning = false;
        telemetry->warnings.pitch_warning = false;
    }

    telemetry->valid = true;
    return true;
}
