#include "bluetooth_manager.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Flash storage offset (last sector before end of flash)
#define BT_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Static state
static BTState current_state = BT_STATE_OFF;
static BTDevice discovered_devices[MAX_BT_DEVICES];
static int device_count = 0;
static int connected_device_idx = -1;
static BTAlertConfig alert_config = {
    .enabled = true,
    .pitch_threshold = 30.0f,
    .bank_threshold = 45.0f,
    .alert_interval_ms = 2000
};
static uint32_t last_alert_time = 0;

// Simulated device database (replace with real BTstack scanning later)
static const char* simulated_device_names[] = {
    "Bose A20",
    "David Clark H10",
    "Lightspeed Zulu",
    "Sennheiser HMEC",
    "Pilot Headset",
    "Aviation Pro"
};

// Initialize Bluetooth subsystem
bool bt_init(void) {
    if (current_state != BT_STATE_OFF) {
        return true;  // Already initialized
    }

    current_state = BT_STATE_INITIALIZING;

    // TODO: Initialize BTstack when ready
    // For now, simulate initialization
    sleep_ms(100);

    device_count = 0;
    connected_device_idx = -1;
    current_state = BT_STATE_IDLE;

    // Try to load previously paired device
    bt_load_paired_device();

    printf("Bluetooth initialized\n");
    return true;
}

// Start scanning for devices
bool bt_start_scan(void) {
    if (current_state == BT_STATE_OFF) {
        if (!bt_init()) return false;
    }

    if (current_state == BT_STATE_SCANNING) {
        return true;  // Already scanning
    }

    current_state = BT_STATE_SCANNING;
    device_count = 0;

    // TODO: Start real BTstack scanning
    // For now, simulate discovering devices
    printf("Starting Bluetooth scan...\n");

    // Simulate discovering 3-6 devices
    int num_devices = 3 + (rand() % 4);
    for (int i = 0; i < num_devices && i < MAX_BT_DEVICES; i++) {
        BTDevice* dev = &discovered_devices[i];

        // Generate fake MAC address
        for (int j = 0; j < BT_ADDR_LEN; j++) {
            dev->address[j] = rand() & 0xFF;
        }

        // Assign name
        snprintf(dev->name, BT_NAME_MAX_LEN, "%s",
                 simulated_device_names[i % 6]);

        // Random signal strength
        dev->rssi = -40 - (rand() % 50);
        dev->is_paired = false;
        dev->device_type = 2;  // Headset

        device_count++;
    }

    printf("Found %d devices\n", device_count);
    return true;
}

// Stop scanning
void bt_stop_scan(void) {
    if (current_state == BT_STATE_SCANNING) {
        current_state = BT_STATE_IDLE;
        printf("Bluetooth scan stopped\n");
    }
}

// Get current state
BTState bt_get_state(void) {
    return current_state;
}

// Get number of discovered devices
int bt_get_device_count(void) {
    return device_count;
}

// Get device by index
BTDevice* bt_get_device(int index) {
    if (index < 0 || index >= device_count) {
        return NULL;
    }
    return &discovered_devices[index];
}

// Pair with a device by index
bool bt_pair_device(int index) {
    if (index < 0 || index >= device_count) {
        return false;
    }

    current_state = BT_STATE_PAIRING;
    printf("Pairing with %s...\n", discovered_devices[index].name);

    // TODO: Implement real BTstack pairing
    // Simulate pairing delay
    sleep_ms(1500);

    // Mark as paired and connected
    discovered_devices[index].is_paired = true;
    connected_device_idx = index;
    current_state = BT_STATE_CONNECTED;

    printf("Paired successfully!\n");

    // Save to flash
    bt_save_paired_device();

    return true;
}

// Disconnect from current device
void bt_disconnect(void) {
    if (connected_device_idx >= 0) {
        printf("Disconnecting from %s\n",
               discovered_devices[connected_device_idx].name);
        discovered_devices[connected_device_idx].is_paired = false;
        connected_device_idx = -1;
        current_state = BT_STATE_IDLE;
    }
}

// Check if connected
bool bt_is_connected(void) {
    return (connected_device_idx >= 0 &&
            current_state == BT_STATE_CONNECTED);
}

// Get connected device name
const char* bt_get_connected_name(void) {
    if (connected_device_idx >= 0 && connected_device_idx < device_count) {
        return discovered_devices[connected_device_idx].name;
    }
    return NULL;
}

// Send audio alert (beep tone)
bool bt_send_alert(uint16_t frequency_hz, uint16_t duration_ms) {
    if (!bt_is_connected()) {
        return false;
    }

    // TODO: Send real audio data via Bluetooth
    // For now, just log
    printf("Sending %dHz alert for %dms to %s\n",
           frequency_hz, duration_ms,
           discovered_devices[connected_device_idx].name);

    return true;
}

// Configure alert thresholds
void bt_configure_alerts(BTAlertConfig* config) {
    if (config) {
        alert_config = *config;
        printf("Alert config: pitch=%.1f° bank=%.1f° interval=%dms\n",
               alert_config.pitch_threshold,
               alert_config.bank_threshold,
               alert_config.alert_interval_ms);
    }
}

// Check attitude and send alert if thresholds exceeded
void bt_check_and_alert(float pitch, float roll) {
    if (!alert_config.enabled || !bt_is_connected()) {
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_alert_time < alert_config.alert_interval_ms) {
        return;  // Too soon since last alert
    }

    float abs_pitch = fabsf(pitch);
    float abs_roll = fabsf(roll);

    if (abs_pitch > alert_config.pitch_threshold ||
        abs_roll > alert_config.bank_threshold) {

        // Determine alert frequency based on severity
        uint16_t freq = 800;  // Base frequency
        if (abs_pitch > alert_config.pitch_threshold) {
            freq = 1000;  // Higher pitch for pitch exceedance
        }
        if (abs_roll > alert_config.bank_threshold) {
            freq = 1200;  // Even higher for bank exceedance
        }

        bt_send_alert(freq, 200);
        last_alert_time = now;
    }
}

// Background processing
void bt_poll(void) {
    // TODO: Process BTstack events
    // For now, nothing to do in simulation mode
}

// Save paired device to flash
bool bt_save_paired_device(void) {
    if (connected_device_idx < 0) {
        return false;
    }

    // Prepare data to save
    uint8_t save_buffer[FLASH_PAGE_SIZE];
    memset(save_buffer, 0xFF, FLASH_PAGE_SIZE);

    // Magic header
    save_buffer[0] = 0xB7;  // 'BT'
    save_buffer[1] = 0xDE;
    save_buffer[2] = 0x01;  // Version 1

    // Copy device info
    memcpy(&save_buffer[4], &discovered_devices[connected_device_idx],
           sizeof(BTDevice));

    // Erase and write flash
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(BT_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(BT_FLASH_OFFSET, save_buffer, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);

    printf("Saved paired device to flash\n");
    return true;
}

// Load paired device from flash
bool bt_load_paired_device(void) {
    const uint8_t* flash_data = (const uint8_t*)(XIP_BASE + BT_FLASH_OFFSET);

    // Check magic header
    if (flash_data[0] != 0xB7 || flash_data[1] != 0xDE ||
        flash_data[2] != 0x01) {
        printf("No saved Bluetooth device found\n");
        return false;
    }

    // Load device info
    if (device_count < MAX_BT_DEVICES) {
        memcpy(&discovered_devices[device_count], &flash_data[4],
               sizeof(BTDevice));
        connected_device_idx = device_count;
        device_count++;
        current_state = BT_STATE_CONNECTED;

        printf("Loaded paired device: %s\n",
               discovered_devices[connected_device_idx].name);
        return true;
    }

    return false;
}
