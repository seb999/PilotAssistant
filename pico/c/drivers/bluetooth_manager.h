#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_BT_DEVICES 10
#define BT_NAME_MAX_LEN 32
#define BT_ADDR_LEN 6

// Bluetooth device structure
typedef struct {
    uint8_t  address[BT_ADDR_LEN];
    char     name[BT_NAME_MAX_LEN];
    int8_t   rssi;
    bool     is_paired;
    uint8_t  device_type;  // 0=Unknown, 1=Audio, 2=Headset, 3=Speaker
} BTDevice;

// Bluetooth manager state
typedef enum {
    BT_STATE_OFF,
    BT_STATE_INITIALIZING,
    BT_STATE_IDLE,
    BT_STATE_SCANNING,
    BT_STATE_PAIRING,
    BT_STATE_CONNECTED,
    BT_STATE_ERROR
} BTState;

// Alert configuration
typedef struct {
    bool     enabled;
    float    pitch_threshold;   // degrees
    float    bank_threshold;    // degrees
    uint16_t alert_interval_ms; // minimum time between alerts
} BTAlertConfig;

// Initialize Bluetooth subsystem
bool bt_init(void);

// Start scanning for devices
bool bt_start_scan(void);

// Stop scanning
void bt_stop_scan(void);

// Get current state
BTState bt_get_state(void);

// Get number of discovered devices
int bt_get_device_count(void);

// Get device by index
BTDevice* bt_get_device(int index);

// Pair with a device by index
bool bt_pair_device(int index);

// Disconnect from current device
void bt_disconnect(void);

// Check if connected
bool bt_is_connected(void);

// Get connected device name (NULL if not connected)
const char* bt_get_connected_name(void);

// Send audio alert (beep tone)
bool bt_send_alert(uint16_t frequency_hz, uint16_t duration_ms);

// Configure alert thresholds
void bt_configure_alerts(BTAlertConfig* config);

// Check attitude and send alert if thresholds exceeded
void bt_check_and_alert(float pitch, float roll);

// Background processing (call periodically)
void bt_poll(void);

// Save paired device to flash
bool bt_save_paired_device(void);

// Load paired device from flash
bool bt_load_paired_device(void);

#endif // BLUETOOTH_MANAGER_H
