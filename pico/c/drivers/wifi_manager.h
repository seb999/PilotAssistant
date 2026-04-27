#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

// Connect to configured hotspot. Blocks up to ~15s. Returns true if connected.
bool wifi_connect(void);

// Returns true if WiFi link is currently up.
bool wifi_is_connected(void);

// Poll the CYW43 driver. Must be called regularly from the main loop.
void wifi_poll(void);

#endif // WIFI_MANAGER_H
