/**
 * Pico2 Menu System
 * Interactive menu with joystick navigation
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "st7789_lcd.h"
#include "img/splash_data.h"
#include "input_handler.h"
#include "menu.h"
#include "telemetry_parser.h"

#define LED_PIN 25
#define BUFFER_SIZE 1024

// Telemetry data and state (used by RADAR)
static TelemetryData latest_telemetry;
static bool telemetry_received = false;
static char rx_buffer[BUFFER_SIZE];
static size_t rx_index = 0;

// Function to draw status icons at the top
void draw_status_icons(void) {
    // Icons are 24x24, positioned at top right with 4px spacing
    // Red when no telemetry or disconnected, green when connected
    bool wifi_ok = telemetry_received && latest_telemetry.status.wifi;
    bool gps_ok = telemetry_received && latest_telemetry.status.gps;
    bool bt_ok = telemetry_received && latest_telemetry.status.bluetooth;

    lcd_draw_wifi_icon(240, 2, wifi_ok);
    lcd_draw_gps_icon(268, 2, gps_ok);
    lcd_draw_bluetooth_icon(296, 2, bt_ok);
}

// Read and process serial telemetry data
void read_telemetry_data() {
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        char ch = (char)c;
        if (ch == '\n' || ch == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                if (parse_telemetry(rx_buffer, &latest_telemetry)) {
                    telemetry_received = true;  // Mark that we've received telemetry
                }
                rx_index = 0;
            }
        } else if (rx_index < BUFFER_SIZE - 1) {
            rx_buffer[rx_index++] = ch;
        } else {
            rx_index = 0;
        }
    }
}

// Menu action functions
void action_go_fly(void) {
    printf("=== GO FLY selected ===\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(80, 100, "GO FLY", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    sleep_ms(2000);
}

void action_bluetooth(void) {
    printf("=== BLUETOOTH selected ===\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(70, 100, "BLUETOOTH", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    sleep_ms(2000);
}

void action_gyro_offset(void) {
    printf("=== GYRO OFFSET selected ===\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string(60, 100, "GYRO OFFSET", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    sleep_ms(2000);
}

// OLD TELEMETRY MENU REMOVED - Use RADAR instead

// Helper function to convert lat/lon distance to pixels on radar
void latlon_to_radar_xy(double own_lat, double own_lon, double ac_lat, double ac_lon,
                        int16_t* out_x, int16_t* out_y, uint16_t radar_center_x, uint16_t radar_center_y) {
    // Earth radius in km
    const double R = 6371.0;

    // Convert to radians
    double lat1 = own_lat * 3.14159265359 / 180.0;
    double lon1 = own_lon * 3.14159265359 / 180.0;
    double lat2 = ac_lat * 3.14159265359 / 180.0;
    double lon2 = ac_lon * 3.14159265359 / 180.0;

    // Calculate x (east-west) and y (north-south) distances in km
    double dx_km = (lon2 - lon1) * cos((lat1 + lat2) / 2.0) * R;
    double dy_km = (lat2 - lat1) * R;

    // Convert km to pixels (scale: 25km = 80 pixels, so 1km = 3.2 pixels)
    const double km_to_pixels = 3.2;
    *out_x = radar_center_x + (int16_t)(dx_km * km_to_pixels);
    *out_y = radar_center_y - (int16_t)(dy_km * km_to_pixels);  // Subtract because screen Y increases downward
}

// Draw static radar elements (called once)
void draw_radar_static(void) {
    const uint16_t radar_center_x = 160;
    const uint16_t radar_center_y = 120;

    // Draw 3 concentric circles with equal spacing (35 pixels each) - use full screen height
    lcd_draw_circle(radar_center_x, radar_center_y, 35, COLOR_WHITE);   // Inner ring
    lcd_draw_circle(radar_center_x, radar_center_y, 70, COLOR_WHITE);   // Middle ring
    lcd_draw_circle(radar_center_x, radar_center_y, 105, COLOR_WHITE);  // Outer ring

    // Draw ownship at center (yellow square)
    lcd_fill_rect(radar_center_x - 2, radar_center_y - 2, 5, 5, COLOR_YELLOW);

    // Draw status icons at top right
    draw_status_icons();
}

// Clear previous aircraft positions
static int16_t prev_aircraft_x[10];
static int16_t prev_aircraft_y[10];
static uint8_t prev_aircraft_count = 0;

// Update radar aircraft positions (called repeatedly)
void update_radar_aircraft(void) {
    char buf[32];
    const uint16_t radar_center_x = 160;
    const uint16_t radar_center_y = 120;

    // Clear previous aircraft positions
    for (int i = 0; i < prev_aircraft_count; i++) {
        if (prev_aircraft_x[i] >= 0 && prev_aircraft_y[i] >= 0) {
            lcd_fill_rect(prev_aircraft_x[i] - 3, prev_aircraft_y[i] - 5, 50, 10, COLOR_BLACK);
        }
    }

    // Draw status icons
    draw_status_icons();

    // Draw new aircraft positions
    prev_aircraft_count = 0;
    for (int i = 0; i < latest_telemetry.traffic_count && i < 10; i++) {
        TrafficData* t = &latest_telemetry.traffic[i];
        int16_t screen_x, screen_y;

        latlon_to_radar_xy(latest_telemetry.own.lat, latest_telemetry.own.lon,
                          t->lat, t->lon, &screen_x, &screen_y,
                          radar_center_x, radar_center_y);

        // Draw aircraft if within screen bounds (expanded for debugging)
        if (screen_x >= 0 && screen_x <= 320 && screen_y >= 0 && screen_y <= 240) {
            // Draw aircraft as red square
            lcd_fill_rect(screen_x - 2, screen_y - 2, 4, 4, COLOR_RED);

            // Draw callsign nearby (only for first 3 to avoid clutter)
            if (i < 3) {
                lcd_draw_string(screen_x + 4, screen_y - 4, t->id, COLOR_RED, COLOR_BLACK);
            }

            // Store position for next clear
            prev_aircraft_x[prev_aircraft_count] = screen_x;
            prev_aircraft_y[prev_aircraft_count] = screen_y;
            prev_aircraft_count++;
        }
    }

    // Draw traffic count at bottom
    snprintf(buf, sizeof(buf), "TRAFFIC: %d", latest_telemetry.traffic_count);
    lcd_fill_rect(5, 220, 150, 10, COLOR_BLACK);  // Clear old count
    lcd_draw_string(5, 220, buf, COLOR_WHITE, COLOR_BLACK);
}

void action_radar(void) {
    printf("=== RADAR selected ===\n");

    telemetry_received = false;

    // Reset previous aircraft tracking
    prev_aircraft_count = 0;

    // Clear screen and draw radar immediately
    lcd_clear(COLOR_BLACK);
    draw_radar_static();

    InputState input_state = {0};

    // Radar display loop
    while (true) {
        input_read(&input_state);

        // Exit on LEFT button press
        if (input_just_pressed_left(&input_state)) {
            printf("Exiting radar display\n");
            break;
        }

        // Read telemetry data
        read_telemetry_data();

        // Update aircraft positions if we have telemetry
        if (telemetry_received) {
            update_radar_aircraft();
        }

        sleep_ms(500);  // Update every 500ms
    }
}

// Menu items (UPPERCASE for font compatibility)
static MenuItem menu_items[] = {
    {"GO FLY", action_go_fly},
    {"BLUETOOTH", action_bluetooth},
    {"GYRO OFFSET", action_gyro_offset},
    {"RADAR", action_radar}
};

int main() {
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // Initialize USB serial (stdio)
    stdio_init_all();

    // Wait for USB connection
    sleep_ms(2000);

    printf("\n\n");
    printf("=====================================\n");
    printf("  Pico2 Menu System v1.0\n");
    printf("  With Joystick Navigation\n");
    printf("=====================================\n");

    // Initialize LCD
    printf("Initializing LCD...\n");
    lcd_init();

    // Display splash screen for 2 seconds
    printf("Displaying splash screen...\n");
    lcd_display_splash(splash_320x240_bin, splash_320x240_bin_len);

    // Flash LED during splash
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }
    sleep_ms(1400);  // Total 2 seconds for splash

    // Initialize input handler
    printf("Initializing input handler...\n");
    input_init();
    printf("Input handler ready\n");

    // Initialize menu
    printf("Initializing menu...\n");
    MenuState menu;
    menu_init(&menu, menu_items, 4, NULL);  // 4 items: GO FLY, BLUETOOTH, GYRO OFFSET, RADAR

    // Draw initial menu
    menu_draw_full(&menu);
    printf("Menu displayed\n\n");

    // Input state
    InputState input_state = {0};

    // Main loop
    while (true) {
        // Read inputs
        input_read(&input_state);

        // Handle menu navigation
        if (menu_handle_input(&menu, &input_state)) {
            // An action was selected and executed
            // Redraw the menu after action completes
            menu_draw_full(&menu);
        }

        // Small delay
        sleep_ms(10);
    }

    return 0;
}
