#ifndef OPENSKY_CLIENT_H
#define OPENSKY_CLIENT_H

#include <stdbool.h>
#include "../src/telemetry_parser.h"

// Default position to use when no GPS fix: Stockholm Arlanda Airport
#define ARLANDA_LAT  59.6519
#define ARLANDA_LON  17.9186
#define ARLANDA_ALT  40.0    // meters AMSL

// Fetch ADS-B traffic from OpenSky around (lat, lon) with ~26km radius.
// Populates td->traffic[] and td->traffic_count.
// Sets td->own to the provided position.
// Must be called from a context where wifi_poll() is being called regularly.
// Returns true on success, false on network/parse error.
bool opensky_fetch(double lat, double lon, TelemetryData *td);

#endif // OPENSKY_CLIENT_H
