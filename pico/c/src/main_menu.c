/**
 * Pico 2W Menu System — standalone, touch + WiFi, no USB telemetry
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "st7789_lcd.h"
#include "img/splash_data.h"
#include "menu.h"
#include "xpt2046_touch.h"
#include "telemetry_parser.h"
#include "icm20948_sensor.h"
#include "madgwick_filter.h"
#include "wifi_manager.h"
#include "opensky_client.h"
#include "bluetooth_manager.h"
#include "ahrs_core.h"

#define LED_PIN 25

// Traffic data populated by OpenSky fetches
static TelemetryData latest_telemetry;

// ── Status ribbon ─────────────────────────────────────────────────────────────

static void draw_ribbon_internal(void) {
    bool wifi_ok = wifi_is_connected();
    bool bt_ok = bt_is_connected();
    lcd_fill_rect(0, 0, 320, 28, COLOR_BLACK);
    lcd_draw_gps_icon(188, 2, false);         // 188-212: no GPS driver yet
    lcd_draw_wifi_icon(216, 2, wifi_ok);      // 216-240
    lcd_draw_bluetooth_icon(244, 2, bt_ok);   // 244-268
    lcd_draw_battery_icon(272, 2, 85);        // 272-296
    lcd_flush_rect(0, 0, 320, 28);
}

void draw_ribbon_force(void) {
    draw_ribbon_internal();
}

void draw_status_icons(void) {
    draw_ribbon_internal();
}

// ── Bluetooth pairing ─────────────────────────────────────────────────────────

#define BT_LIST_Y_START 75
#define BT_LIST_ITEM_H  30
#define BT_SCAN_BTN_Y   35
#define BT_SCAN_BTN_H   28

static void bt_draw_static(void) {
    lcd_clear(COLOR_BLACK);
    draw_ribbon_force();

    // Title
    lcd_draw_string_scaled(60, 32, "BLUETOOTH", COLOR_YELLOW, COLOR_BLACK, 2);

    // Scan button
    lcd_fill_round_rect(10, BT_SCAN_BTN_Y, 130, BT_SCAN_BTN_H, 6, 0x041F);
    lcd_draw_string(25, BT_SCAN_BTN_Y + 10, "SCAN DEVICES", COLOR_WHITE, 0x041F);

    // Config button
    lcd_fill_round_rect(150, BT_SCAN_BTN_Y, 160, BT_SCAN_BTN_H, 6, 0xFD20);
    lcd_draw_string(165, BT_SCAN_BTN_Y + 10, "ALERT CONFIG", COLOR_WHITE, 0xFD20);
}

static void bt_draw_devices(void) {
    const uint16_t bg = COLOR_BLACK;
    int dev_count = bt_get_device_count();

    // Clear device list area
    lcd_fill_rect(0, BT_LIST_Y_START, 320, LCD_HEIGHT - BT_LIST_Y_START, bg);

    if (dev_count == 0) {
        lcd_draw_string(70, 120, "No devices found", COLOR_CYAN, bg);
        lcd_draw_string(50, 140, "Tap SCAN to search", COLOR_WHITE, bg);
    } else {
        // Draw up to 5 devices
        for (int i = 0; i < dev_count && i < 5; i++) {
            BTDevice* dev = bt_get_device(i);
            if (!dev) continue;

            uint16_t y = BT_LIST_Y_START + i * BT_LIST_ITEM_H;
            uint16_t item_color = dev->is_paired ? 0x07E0 : 0x2104;

            // Device box
            lcd_fill_round_rect(5, y, 310, BT_LIST_ITEM_H - 2, 5, item_color);

            // Device name
            lcd_draw_string(12, y + 5, dev->name, COLOR_WHITE, item_color);

            // Signal strength indicator
            char rssi_str[8];
            snprintf(rssi_str, sizeof(rssi_str), "%ddBm", dev->rssi);
            lcd_draw_string(12, y + 17, rssi_str, COLOR_CYAN, item_color);

            // Status indicator
            if (dev->is_paired) {
                lcd_draw_string(250, y + 10, "PAIRED", COLOR_YELLOW, item_color);
            }
        }

        if (dev_count > 5) {
            lcd_draw_string(100, 225, "Scroll for more...", COLOR_CYAN, bg);
        }
    }
}

static void bt_draw_status(const char* msg, uint16_t color) {
    lcd_fill_rect(0, 220, 320, 20, COLOR_BLACK);
    int x = (320 - strlen(msg) * 6) / 2;
    lcd_draw_string(x, 222, msg, color, COLOR_BLACK);
}

static void bt_show_config_dialog(void) {
    // Static alert configuration
    static float pitch_thresh = 30.0f;
    static float bank_thresh = 45.0f;
    static uint16_t alert_interval = 2000;

    bool was_touched = false;
    bool redraw = true;

    while (true) {
        if (redraw) {
            // Config dialog overlay
            lcd_fill_round_rect(20, 60, 280, 140, 8, 0x2104);
            // Draw border
            lcd_draw_line(20, 60, 299, 60, COLOR_WHITE);      // top
            lcd_draw_line(20, 199, 299, 199, COLOR_WHITE);    // bottom
            lcd_draw_line(20, 60, 20, 199, COLOR_WHITE);      // left
            lcd_draw_line(299, 60, 299, 199, COLOR_WHITE);    // right

            lcd_draw_string_scaled(70, 70, "ALERT CONFIG", COLOR_YELLOW, 0x2104, 2);

            // Pitch threshold with +/- buttons
            lcd_draw_string(30, 100, "Pitch:", COLOR_CYAN, 0x2104);
            char buf[20];
            snprintf(buf, sizeof(buf), "%.0fdeg", pitch_thresh);
            lcd_draw_string(90, 100, buf, COLOR_WHITE, 0x2104);
            lcd_fill_round_rect(180, 96, 25, 18, 3, 0x07E0);  // - button
            lcd_draw_string(188, 100, "-", COLOR_BLACK, 0x07E0);
            lcd_fill_round_rect(210, 96, 25, 18, 3, 0x07E0);  // + button
            lcd_draw_string(218, 100, "+", COLOR_BLACK, 0x07E0);

            // Bank threshold with +/- buttons
            lcd_draw_string(30, 125, "Bank:", COLOR_CYAN, 0x2104);
            snprintf(buf, sizeof(buf), "%.0fdeg", bank_thresh);
            lcd_draw_string(90, 125, buf, COLOR_WHITE, 0x2104);
            lcd_fill_round_rect(180, 121, 25, 18, 3, 0x07E0);  // - button
            lcd_draw_string(188, 125, "-", COLOR_BLACK, 0x07E0);
            lcd_fill_round_rect(210, 121, 25, 18, 3, 0x07E0);  // + button
            lcd_draw_string(218, 125, "+", COLOR_BLACK, 0x07E0);

            // Alert interval with +/- buttons
            lcd_draw_string(30, 150, "Interval:", COLOR_CYAN, 0x2104);
            snprintf(buf, sizeof(buf), "%dms", alert_interval);
            lcd_draw_string(110, 150, buf, COLOR_WHITE, 0x2104);
            lcd_fill_round_rect(180, 146, 25, 18, 3, 0x07E0);  // - button
            lcd_draw_string(188, 150, "-", COLOR_BLACK, 0x07E0);
            lcd_fill_round_rect(210, 146, 25, 18, 3, 0x07E0);  // + button
            lcd_draw_string(218, 150, "+", COLOR_BLACK, 0x07E0);

            // Close button
            lcd_fill_round_rect(100, 172, 120, 22, 5, 0xFD20);
            lcd_draw_string(140, 178, "CLOSE", COLOR_WHITE, 0xFD20);

            lcd_flush();
            redraw = false;
        }

        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);

        if (touched && !was_touched) {
            // Pitch - button
            if (tx >= 180 && tx < 205 && ty >= 96 && ty < 114) {
                pitch_thresh = (pitch_thresh > 10.0f) ? pitch_thresh - 5.0f : pitch_thresh;
                redraw = true;
            }
            // Pitch + button
            else if (tx >= 210 && tx < 235 && ty >= 96 && ty < 114) {
                pitch_thresh = (pitch_thresh < 60.0f) ? pitch_thresh + 5.0f : pitch_thresh;
                redraw = true;
            }
            // Bank - button
            else if (tx >= 180 && tx < 205 && ty >= 121 && ty < 139) {
                bank_thresh = (bank_thresh > 15.0f) ? bank_thresh - 5.0f : bank_thresh;
                redraw = true;
            }
            // Bank + button
            else if (tx >= 210 && tx < 235 && ty >= 121 && ty < 139) {
                bank_thresh = (bank_thresh < 90.0f) ? bank_thresh + 5.0f : bank_thresh;
                redraw = true;
            }
            // Interval - button
            else if (tx >= 180 && tx < 205 && ty >= 146 && ty < 164) {
                alert_interval = (alert_interval > 500) ? alert_interval - 500 : alert_interval;
                redraw = true;
            }
            // Interval + button
            else if (tx >= 210 && tx < 235 && ty >= 146 && ty < 164) {
                alert_interval = (alert_interval < 5000) ? alert_interval + 500 : alert_interval;
                redraw = true;
            }
            // Close button
            else if (tx >= 100 && tx < 220 && ty >= 172 && ty < 194) {
                // Save configuration to Bluetooth manager
                BTAlertConfig cfg = {
                    .enabled = true,
                    .pitch_threshold = pitch_thresh,
                    .bank_threshold = bank_thresh,
                    .alert_interval_ms = alert_interval
                };
                bt_configure_alerts(&cfg);
                break;
            }
        }

        was_touched = touched;
        sleep_ms(20);
    }
}

void action_bluetooth(void) {
    bt_init();

    bt_draw_static();
    bt_draw_devices();
    lcd_flush();

    bool was_touched = false;
    bool scanning = false;

    while (true) {
        bt_poll();

        // Check if scanning just completed
        BTState state = bt_get_state();
        if (scanning && state == BT_STATE_IDLE) {
            scanning = false;
            bt_draw_devices();
            bt_draw_status("Scan complete", COLOR_GREEN);
            lcd_flush();
        }

        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);

        if (touched && !was_touched) {
            // Tap ribbon → back to menu
            if (ty < 28) {
                break;
            }
            // Tap scan button
            else if (ty >= BT_SCAN_BTN_Y && ty < BT_SCAN_BTN_Y + BT_SCAN_BTN_H) {
                if (tx >= 10 && tx < 140) {
                    bt_start_scan();
                    scanning = true;
                    bt_draw_status("Scanning...", COLOR_YELLOW);
                    lcd_flush();
                }
                // Tap config button
                else if (tx >= 150 && tx < 310) {
                    bt_show_config_dialog();
                    bt_draw_static();
                    bt_draw_devices();
                    lcd_flush();
                }
            }
            // Tap device list item
            else if (ty >= BT_LIST_Y_START) {
                int idx = (ty - BT_LIST_Y_START) / BT_LIST_ITEM_H;
                if (idx >= 0 && idx < bt_get_device_count() && idx < 5) {
                    BTDevice* dev = bt_get_device(idx);
                    if (dev) {
                        if (dev->is_paired) {
                            // Disconnect
                            bt_disconnect();
                            bt_draw_status("Disconnected", COLOR_RED);
                        } else {
                            // Pair
                            bt_draw_status("Pairing...", COLOR_YELLOW);
                            lcd_flush();

                            if (bt_pair_device(idx)) {
                                bt_draw_status("Paired successfully!", COLOR_GREEN);
                            } else {
                                bt_draw_status("Pairing failed", COLOR_RED);
                            }
                        }
                        bt_draw_devices();
                        lcd_flush();
                    }
                }
            }
        }

        was_touched = touched;
        sleep_ms(20);
    }
}

// ── Radar ─────────────────────────────────────────────────────────────────────

#define RDR_CX   106
#define RDR_CY   133
#define RDR_R1    28
#define RDR_R2    56
#define RDR_R3    84
#define RDR_PX   213
#define RDR_PANEL_BG  0x2104
#define KM_TO_PX  3.23f

#define OPENSKY_INTERVAL_MS  15000

static int16_t  prev_ac_x[MAX_TRAFFIC];
static int16_t  prev_ac_y[MAX_TRAFFIC];
static uint8_t  prev_ac_ti[MAX_TRAFFIC];
static uint8_t  prev_ac_count = 0;
static int      radar_selected = -1;

static void radar_draw_static(void) {
    lcd_clear(COLOR_BLACK);
    lcd_fill_rect(RDR_PX, 0, 320 - RDR_PX, LCD_HEIGHT, RDR_PANEL_BG);
    lcd_draw_line(RDR_PX, 0, RDR_PX, LCD_HEIGHT - 1, COLOR_WHITE);
    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R1, COLOR_WHITE);
    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R2, COLOR_WHITE);
    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R3, COLOR_WHITE);
    lcd_fill_rect(RDR_CX - 2, RDR_CY - 2, 5, 5, COLOR_YELLOW);
    lcd_fill_rect(RDR_CX - 1, RDR_CY - RDR_R3 - 6, 3, 6, COLOR_WHITE);
    draw_ribbon_force();
}

#define RDR_BTN_Y    200   // y where nav buttons start
#define RDR_BTN_MID  (RDR_PX + (319 - RDR_PX) / 2)  // x midpoint of panel

static void radar_draw_nav_buttons(void) {
    const uint16_t bg = RDR_PANEL_BG;
    // Separator line above buttons
    lcd_draw_line(RDR_PX + 1, RDR_BTN_Y, 319, RDR_BTN_Y, COLOR_WHITE);
    // Vertical divider between buttons
    lcd_draw_line(RDR_BTN_MID, RDR_BTN_Y, RDR_BTN_MID, LCD_HEIGHT - 1, COLOR_WHITE);

    // "< PRV" left button
    lcd_fill_rect(RDR_PX + 2, RDR_BTN_Y + 1, RDR_BTN_MID - RDR_PX - 2, LCD_HEIGHT - RDR_BTN_Y - 1, bg);
    lcd_draw_string(RDR_PX + 6,  RDR_BTN_Y + 12, "<",   COLOR_WHITE, bg);
    lcd_draw_string(RDR_PX + 16, RDR_BTN_Y + 12, "PRV", COLOR_WHITE, bg);

    // "NXT >" right button
    lcd_fill_rect(RDR_BTN_MID + 1, RDR_BTN_Y + 1, 319 - RDR_BTN_MID, LCD_HEIGHT - RDR_BTN_Y - 1, bg);
    lcd_draw_string(RDR_BTN_MID + 4,  RDR_BTN_Y + 12, "NXT", COLOR_WHITE, bg);
    lcd_draw_string(RDR_BTN_MID + 22, RDR_BTN_Y + 12, ">",   COLOR_WHITE, bg);
}

static void radar_draw_panel(void) {
    const uint16_t px = RDR_PX + 3;
    const uint16_t bg = RDR_PANEL_BG;
    // Clear info area (above nav buttons)
    lcd_fill_rect(RDR_PX + 1, 29, 319 - RDR_PX, RDR_BTN_Y - 29, bg);

    if (radar_selected < 0 || radar_selected >= latest_telemetry.traffic_count) {
        char buf[16];
        snprintf(buf, sizeof(buf), "TFC: %d", latest_telemetry.traffic_count);
        lcd_draw_string(px,  50, buf,     COLOR_CYAN,  bg);
        lcd_draw_string(px,  90, "Tap",   COLOR_WHITE, bg);
        lcd_draw_string(px, 103, "a blip",COLOR_WHITE, bg);
        lcd_draw_string(px, 120, "or use", COLOR_WHITE, bg);
        lcd_draw_string(px, 133, "arrows", COLOR_WHITE, bg);
    } else {
        TrafficData *t = &latest_telemetry.traffic[radar_selected];

        // Callsign — big, yellow
        lcd_draw_string_scaled(px, 34, t->id, COLOR_YELLOW, bg, 2);

        // Counter: which / total
        char idx_buf[12];
        snprintf(idx_buf, sizeof(idx_buf), "%d/%d",
                 radar_selected + 1, latest_telemetry.traffic_count);
        lcd_draw_string(px, 58, idx_buf, COLOR_WHITE, bg);

        // Distance
        const double R = 6371.0;
        double lat1r = latest_telemetry.own.lat * M_PI / 180.0;
        double lon1r = latest_telemetry.own.lon * M_PI / 180.0;
        double lat2r = t->lat * M_PI / 180.0;
        double lon2r = t->lon * M_PI / 180.0;
        double dx_km = (lon2r - lon1r) * cos((lat1r + lat2r) / 2.0) * R;
        double dy_km = (lat2r - lat1r) * R;
        double dist  = sqrt(dx_km * dx_km + dy_km * dy_km);

        char buf[20];

        lcd_draw_string(px,  80, "HDG", COLOR_CYAN,  bg);
        snprintf(buf, sizeof(buf), "%.0f deg", t->heading);
        lcd_draw_string(px,  92, buf,   COLOR_WHITE, bg);

        lcd_draw_string(px, 110, "ALT", COLOR_CYAN,  bg);
        snprintf(buf, sizeof(buf), "%d ft", (int)t->alt);
        lcd_draw_string(px, 122, buf,   COLOR_WHITE, bg);

        lcd_draw_string(px, 140, "SPD", COLOR_CYAN,  bg);
        snprintf(buf, sizeof(buf), "%d kt", (int)t->speed);
        lcd_draw_string(px, 152, buf,   COLOR_WHITE, bg);

        lcd_draw_string(px, 170, "DST", COLOR_CYAN,  bg);
        snprintf(buf, sizeof(buf), "%.1f km", dist);
        lcd_draw_string(px, 182, buf,   COLOR_WHITE, bg);
    }

    radar_draw_nav_buttons();
}

static void radar_panel_fetching(void) {
    const uint16_t px = RDR_PX + 3;
    lcd_fill_rect(RDR_PX + 1, 29, 319 - RDR_PX, LCD_HEIGHT - 29, RDR_PANEL_BG);
    lcd_draw_string(px, 80, "FETCHING", COLOR_CYAN, RDR_PANEL_BG);
    lcd_draw_string(px, 95, "OPENSKY",  COLOR_CYAN, RDR_PANEL_BG);
    lcd_flush_rect(RDR_PX, 0, 320 - RDR_PX, LCD_HEIGHT);
}

static void radar_update_blips(void) {
    for (int i = 0; i < prev_ac_count; i++)
        lcd_fill_rect(prev_ac_x[i] - 2, prev_ac_y[i] - 2, 4 + 7*6 + 2, 13, COLOR_BLACK);

    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R1, COLOR_WHITE);
    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R2, COLOR_WHITE);
    lcd_draw_circle(RDR_CX, RDR_CY, RDR_R3, COLOR_WHITE);
    lcd_fill_rect(RDR_CX - 2, RDR_CY - 2, 5, 5, COLOR_YELLOW);

    prev_ac_count = 0;
    for (int i = 0; i < latest_telemetry.traffic_count && i < MAX_TRAFFIC; i++) {
        TrafficData *t = &latest_telemetry.traffic[i];
        const double R = 6371.0;
        double lat1r = latest_telemetry.own.lat * M_PI / 180.0;
        double lon1r = latest_telemetry.own.lon * M_PI / 180.0;
        double lat2r = t->lat * M_PI / 180.0;
        double lon2r = t->lon * M_PI / 180.0;
        double dx_km = (lon2r - lon1r) * cos((lat1r + lat2r) / 2.0) * R;
        double dy_km = (lat2r - lat1r) * R;

        int16_t sx = RDR_CX + (int16_t)(dx_km * KM_TO_PX);
        int16_t sy = RDR_CY - (int16_t)(dy_km * KM_TO_PX);
        if (sx < 4 || sx > RDR_PX - 4 || sy < 29 || sy > LCD_HEIGHT - 4) continue;

        bool sel = (i == radar_selected);
        uint16_t color = sel ? COLOR_YELLOW : COLOR_RED;
        lcd_fill_rect(sx - 2, sy - 2, 5, 5, color);
        lcd_draw_string(sx + 4, sy - 4, t->id, color, COLOR_BLACK);

        prev_ac_x[prev_ac_count]  = sx;
        prev_ac_y[prev_ac_count]  = sy;
        prev_ac_ti[prev_ac_count] = (uint8_t)i;
        prev_ac_count++;
    }

    char buf[20];
    snprintf(buf, sizeof(buf), "TFC %d", latest_telemetry.traffic_count);
    lcd_fill_rect(0, 226, 80, 10, COLOR_BLACK);
    lcd_draw_string(2, 226, buf, COLOR_WHITE, COLOR_BLACK);
}

void action_radar(void) {
    prev_ac_count  = 0;
    radar_selected = -1;
    memset(&latest_telemetry, 0, sizeof(latest_telemetry));

    radar_draw_static();
    radar_draw_panel();
    lcd_flush();

    bool was_touched = false;
    uint32_t last_opensky_fetch = 0;  // force immediate fetch on entry

    while (true) {
        wifi_poll();

        if (wifi_is_connected()) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_opensky_fetch >= OPENSKY_INTERVAL_MS) {
                last_opensky_fetch = now;

                double fetch_lat = ARLANDA_LAT;
                double fetch_lon = ARLANDA_LON;

                radar_panel_fetching();

                TelemetryData sky = {0};
                if (opensky_fetch(fetch_lat, fetch_lon, &sky)) {
                    latest_telemetry.traffic_count = sky.traffic_count;
                    memcpy(latest_telemetry.traffic, sky.traffic,
                           sky.traffic_count * sizeof(TrafficData));
                    latest_telemetry.own = sky.own;
                }

                radar_update_blips();
                radar_draw_panel();
                lcd_flush();
            }
        }

        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched) {
            if (ty < 28) {
                break;  // Tap ribbon → back to menu
            } else if (tx > RDR_PX) {
                int n = latest_telemetry.traffic_count;
                if (ty >= RDR_BTN_Y && n > 0) {
                    // Nav buttons: prev / next
                    if (tx < RDR_BTN_MID) {
                        radar_selected = (radar_selected <= 0) ? n - 1 : radar_selected - 1;
                    } else {
                        radar_selected = (radar_selected >= n - 1) ? 0 : radar_selected + 1;
                    }
                } else {
                    // Tap info area → deselect
                    radar_selected = -1;
                }
                radar_draw_panel();
                lcd_flush_rect(RDR_PX, 0, 320 - RDR_PX, LCD_HEIGHT);
            } else {
                // Tap radar zone → find nearest blip
                int best = -1, best_d2 = 20 * 20;
                for (int i = 0; i < prev_ac_count; i++) {
                    int ddx = (int)tx - prev_ac_x[i];
                    int ddy = (int)ty - prev_ac_y[i];
                    int d2 = ddx * ddx + ddy * ddy;
                    if (d2 < best_d2) { best_d2 = d2; best = i; }
                }
                radar_selected = (best >= 0) ? prev_ac_ti[best] : -1;
                radar_draw_panel();
                lcd_flush_rect(RDR_PX, 0, 320 - RDR_PX, LCD_HEIGHT);
            }
        }
        was_touched = touched;

        sleep_ms(50);
    }
}

// ── AHRS attitude indicator (real ICM20948 sensor) ────────────────────────────

#define D2R (M_PI / 180.0f)
#define PX_PER_DEG 2.5f

// Draw artificial horizon background
static void draw_horizon_bg(float roll_rad, float pitch_px) {
    const int16_t cx = 160, cy = 120;
    const float sin_r = sinf(roll_rad);
    const float cos_r = cosf(roll_rad);

    // Sky blue and darker brown earth
    const uint16_t sky_color = 0x4D9F;
    const uint16_t earth_color = 0x6180;  // Darker brown

    for (int16_t py = 0; py < LCD_HEIGHT; py++) {
        for (int16_t px = 0; px < LCD_WIDTH; px++) {
            float dx = px - cx;
            float dy = py - cy;
            float rotated_y = dx * sin_r + dy * cos_r;
            float world_y = rotated_y - pitch_px;

            uint16_t color = (world_y < 0) ? sky_color : earth_color;
            lcd_draw_pixel(px, py, color);
        }
    }
}

// Draw pitch ladder
static void draw_pitch_ladder(float roll_rad, float pitch_deg) {
    const int16_t cx = 160, cy = 120;
    const float sin_r = sinf(roll_rad);
    const float cos_r = cosf(roll_rad);

    // Draw pitch lines every 10 degrees - lines rotate with bank to stay parallel to horizon
    for (int p = -60; p <= 60; p += 10) {
        if (p == 0) continue;  // Skip horizon (already drawn by background)

        float pitch_offset = (pitch_deg - p) * PX_PER_DEG;

        // Draw line from left to right, rotated by bank angle
        int16_t x1 = cx - 40, x2 = cx + 40;

        // Rotate the line endpoints around center based on bank angle (negate sin for correct direction)
        int16_t y1 = cy + (int16_t)(-(x1 - cx) * sin_r + pitch_offset * cos_r);
        int16_t y2 = cy + (int16_t)(-(x2 - cx) * sin_r + pitch_offset * cos_r);

        if ((y1 >= 0 && y1 < LCD_HEIGHT) || (y2 >= 0 && y2 < LCD_HEIGHT)) {
            lcd_draw_line(x1, y1, x2, y2, COLOR_WHITE);

            // Label at right end
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%d", p);
            if (p > 0) {
                lcd_draw_string(x2 + 5, y2 - 3, lbl, COLOR_CYAN, COLOR_BLACK);
            } else {
                lcd_draw_string(x2 + 5, y2 - 3, lbl, COLOR_YELLOW, COLOR_BLACK);
            }
        }
    }
}

// Draw bank angle arc
static void draw_bank_arc(float roll_deg) {
    const int16_t cx = 160, cy = 120;
    const int radius = 90;

    // Draw arc markings at -60, -45, -30, -20, -10, 0, +10, +20, +30, +45, +60
    int marks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int i = 0; i < 11; i++) {
        float angle = marks[i] * D2R;
        int16_t x = cx + (int16_t)(radius * sinf(angle));
        int16_t y = cy - (int16_t)(radius * cosf(angle));

        uint16_t color = (marks[i] == 0) ? COLOR_YELLOW : COLOR_WHITE;
        lcd_fill_rect(x - 1, y - 1, 3, 3, color);
    }

    // Draw current bank indicator (red triangle)
    float angle = roll_deg * D2R;
    int16_t x = cx + (int16_t)(radius * sinf(angle));
    int16_t y = cy - (int16_t)(radius * cosf(angle));
    lcd_fill_rect(x - 2, y - 2, 5, 5, COLOR_RED);
}

void action_test_gyro(void) {
    // ========== DUAL-CORE AHRS MODE ==========
    // Core 0: Dedicated AHRS (runs continuously in background, started at boot)
    // Core 1: Display and UI (this core)

    // Main display loop (Core 1)
    AHRSAttitude attitude;
    uint8_t disp_ctr = 0;
    bool was_touched = false;
    const int16_t center_x = 160, center_y = 120;

    while (true) {
        // Get attitude from Core 0
        if (!ahrs_core_get_attitude(&attitude)) {
            // AHRS not healthy
            lcd_clear(COLOR_BLACK);
            lcd_draw_string_scaled(50, 100, "CORE 0 ERROR", COLOR_RED, COLOR_BLACK, 2);
            lcd_flush();
            sleep_ms(1000);
            break;
        }

        // Check for touch to exit
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched && ty < 28) {
            // Tap ribbon to exit
            break;
        }
        was_touched = touched;

        // Keep WiFi alive while in AHRS
        wifi_poll();

        // Render at 10 Hz (every 10th iteration, ~100ms)
        if (++disp_ctr < 10) {
            sleep_ms(10);
            continue;
        }
        disp_ctr = 0;

        // Draw AHRS display
        float roll_rad = attitude.roll * D2R;
        float pitch_px = -attitude.pitch * PX_PER_DEG;

        draw_horizon_bg(roll_rad, pitch_px);
        draw_pitch_ladder(roll_rad, attitude.pitch);
        draw_bank_arc(attitude.roll);

        // Aircraft reference symbol (yellow)
        lcd_draw_line(center_x - 50, center_y, center_x - 10, center_y, COLOR_YELLOW);
        lcd_draw_line(center_x + 10, center_y, center_x + 50, center_y, COLOR_YELLOW);
        lcd_draw_line(center_x - 50, center_y + 1, center_x - 10, center_y + 1, COLOR_YELLOW);
        lcd_draw_line(center_x + 10, center_y + 1, center_x + 50, center_y + 1, COLOR_YELLOW);
        lcd_fill_rect(center_x - 3, center_y - 3, 7, 7, COLOR_YELLOW);

        // Draw status icons LAST so they overlay on top (WiFi, BT, Battery - no ribbon bar)
        bool wifi_ok = wifi_is_connected();
        bool bt_ok = bt_is_connected();
        lcd_draw_wifi_icon(216, 2, wifi_ok);
        lcd_draw_bluetooth_icon(244, 2, bt_ok);
        lcd_draw_battery_icon(272, 2, 85);

        // Minimal status display at bottom (small, non-intrusive)
        char buf[48];

        // Bottom left: Attitude values
        snprintf(buf, sizeof(buf), "R%+.1f P%+.1f", attitude.roll, attitude.pitch);
        lcd_fill_rect(0, 227, 90, 10, COLOR_BLACK);
        lcd_draw_string(4, 227, buf, COLOR_WHITE, COLOR_BLACK);

        // Bottom right: Calibration indicator only
        if (attitude.stationary) {
            lcd_draw_string(280, 227, "CAL", COLOR_GREEN, COLOR_BLACK);
        } else {
            lcd_fill_rect(280, 227, 40, 10, COLOR_BLACK);
        }

        lcd_flush();
    }

    // AHRS continues running on Core 0 in background
    // Just return to menu (no shutdown - instant restart when re-entering AHRS)
}

// ── Menu definition ───────────────────────────────────────────────────────────

#define ICON_COLOR_FLY   0x041F   // Blue
#define ICON_COLOR_BT    0x601F   // Magenta
#define ICON_COLOR_RADAR 0x07C0   // Cyan

static MenuItem menu_items[] = {
    {"GO FLY",   ICON_COLOR_FLY,   action_test_gyro},  // Opens AHRS
    {"BLUETOOTH",ICON_COLOR_BT,    action_bluetooth},
    {"RADAR",    ICON_COLOR_RADAR, action_radar}
};

// ── Main ──────────────────────────────────────────────────────────────────────

int main(void) {
    set_sys_clock_khz(200000, true);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    stdio_init_all();
    sleep_ms(1000);

    printf("=== Pico 2W PilotAssistant ===\n");

    lcd_init();
    lcd_display_splash(splash_320x240_bin, splash_320x240_bin_len);

    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1); sleep_ms(100);
        gpio_put(LED_PIN, 0); sleep_ms(100);
    }

    // Connect to WiFi while splash is shown
    wifi_connect();

    // Initialize Bluetooth
    bt_init();

    // Start AHRS on Core 0 (runs continuously in background)
    printf("Starting AHRS on Core 0...\n");
    ahrs_core_start();

    touch_init();

    icon_menu_draw(menu_items, ICON_COUNT);

    bool was_touched = false;

    while (true) {
        // Touch first — before wifi_poll which can block for tens of ms
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched) {
            int idx = icon_menu_hit_test(tx, ty);
            if (idx >= 0) {
                icon_menu_flash(idx);
                menu_items[idx].action();
                // After returning from action, reset touch state to avoid phantom re-trigger
                was_touched = true;
                icon_menu_draw(menu_items, ICON_COUNT);
                continue;
            }
        }
        was_touched = touched;

        wifi_poll();
        bt_poll();

        // Refresh WiFi/BT icons in ribbon if state changed
        static uint32_t last_ribbon_ms = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_ribbon_ms >= 200) {
            draw_status_icons();
            last_ribbon_ms = now;
        }

        sleep_ms(5);
    }

    return 0;
}
