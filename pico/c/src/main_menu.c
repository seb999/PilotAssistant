/**
 * Pico 2W Menu System — standalone, touch + WiFi, no USB telemetry
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
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

#define LED_PIN 25

// Traffic data populated by OpenSky fetches
static TelemetryData latest_telemetry;

// ── Status ribbon ─────────────────────────────────────────────────────────────

static bool last_wifi_ok = false;

static void draw_ribbon_internal(void) {
    bool wifi_ok = wifi_is_connected();
    last_wifi_ok = wifi_ok;
    lcd_fill_rect(0, 0, 320, 28, 0x2104);
    lcd_draw_wifi_icon(240, 2, wifi_ok);
    lcd_draw_gps_icon(268, 2, false);
    lcd_draw_bluetooth_icon(296, 2, false);
    lcd_flush_rect(0, 0, 320, 28);
}

void draw_ribbon_force(void) {
    draw_ribbon_internal();
}

void draw_status_icons(void) {
    bool wifi_ok = wifi_is_connected();
    if (wifi_ok == last_wifi_ok) return;
    draw_ribbon_internal();
}

// ── Menu action stubs ─────────────────────────────────────────────────────────

void action_go_fly(void) {
    lcd_clear(COLOR_BLACK);
    draw_ribbon_force();
    lcd_draw_string(80, 100, "GO FLY", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    bool was_touched = false;
    while (true) {
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched && ty < 28) break;
        was_touched = touched;
        wifi_poll();
        sleep_ms(20);
    }
}

void action_bluetooth(void) {
    lcd_clear(COLOR_BLACK);
    draw_ribbon_force();
    lcd_draw_string(70, 100, "BLUETOOTH", COLOR_YELLOW, COLOR_BLACK);
    lcd_draw_string(60, 120, "Coming Soon", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    bool was_touched = false;
    while (true) {
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched && ty < 28) break;
        was_touched = touched;
        wifi_poll();
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

// ── Attitude indicator (simulated) ────────────────────────────────────────────

static void ai_draw_bg(float roll_rad, float pitch_px) {
    const int16_t cx = 160, cy = 120;
    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);
    for (int16_t y = 0; y < LCD_HEIGHT; y++) {
        float dy = (float)(y - cy) + pitch_px;
        if (fabsf(sin_r) < 0.01f) {
            lcd_fill_rect(0, y, LCD_WIDTH, 1, (dy < 0) ? COLOR_SKY : COLOR_BROWN);
        } else {
            int16_t xc = cx - (int16_t)(dy * cos_r / sin_r);
            float tl = cos_r * dy + sin_r * (float)(0 - cx);
            uint16_t lc = (tl < 0) ? COLOR_SKY : COLOR_BROWN;
            uint16_t rc = (tl < 0) ? COLOR_BROWN : COLOR_SKY;
            if      (xc <= 0)         lcd_fill_rect(0,  y, LCD_WIDTH,      1, rc);
            else if (xc >= LCD_WIDTH) lcd_fill_rect(0,  y, LCD_WIDTH,      1, lc);
            else {
                lcd_fill_rect(0,  y, xc,             1, lc);
                lcd_fill_rect(xc, y, LCD_WIDTH - xc, 1, rc);
            }
        }
    }
}

static void ai_draw_pitch_ladder(float roll_rad, float pitch) {
    const int16_t cx = 160, cy = 120;
    const float PX_PER_DEG = 2.5f;
    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);
    for (int pm = -60; pm <= 60; pm += 10) {
        float dym = ((float)pm - pitch) * PX_PER_DEG;
        float scx = (float)cx - sin_r * dym;
        float scy = (float)cy + cos_r * dym;
        if (scy < 5.0f || scy >= (LCD_HEIGHT - 20)) continue;
        float hw = (pm == 0) ? 55.0f : (pm % 20 == 0) ? 35.0f : 22.0f;
        int16_t x1 = (int16_t)(scx - cos_r * hw), y1 = (int16_t)(scy - sin_r * hw);
        int16_t x2 = (int16_t)(scx + cos_r * hw), y2 = (int16_t)(scy + sin_r * hw);
        lcd_draw_line(x1, y1, x2, y2, COLOR_WHITE);
        if (pm == 0) lcd_draw_line(x1, y1+1, x2, y2+1, COLOR_WHITE);
        if (pm != 0 && pm % 20 == 0) {
            char lbl[5];
            snprintf(lbl, sizeof(lbl), "%d", abs(pm));
            int16_t lx = x1 - 18;
            if (lx < 0) lx = 0;
            lcd_draw_string(lx, y1 - 3, lbl, COLOR_WHITE, COLOR_BLACK);
        }
    }
}

static void ai_draw_bank_arc(float roll) {
    const float ACX = 160.0f, ACY = -70.0f, ACR = 100.0f;
    const float D2R = (float)M_PI / 180.0f;
    const int ticks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int i = 0; i < 11; i++) {
        float ang = (float)ticks[i] * D2R;
        float nx = sinf(ang), ny = cosf(ang);
        float ax = ACX + ACR * nx, ay = ACY + ACR * ny;
        if (ay < 0.0f || ay >= LCD_HEIGHT) continue;
        float tlen = (ticks[i] % 30 == 0) ? 10.0f : 6.0f;
        lcd_draw_line((int16_t)ax, (int16_t)ay,
                      (int16_t)(ax + nx * tlen), (int16_t)(ay + ny * tlen),
                      COLOR_WHITE);
    }
    float ang = roll * D2R;
    float nx = sinf(ang), ny = cosf(ang);
    float tx = cosf(ang), ty = -sinf(ang);
    float bx = ACX + ACR * nx, by = ACY + ACR * ny;
    if (by >= 0.0f && by < LCD_HEIGHT) {
        int16_t ax = (int16_t)bx,  ay = (int16_t)by;
        int16_t b1x = (int16_t)(bx + nx*11.0f + tx*5.5f);
        int16_t b1y = (int16_t)(by + ny*11.0f + ty*5.5f);
        int16_t b2x = (int16_t)(bx + nx*11.0f - tx*5.5f);
        int16_t b2y = (int16_t)(by + ny*11.0f - ty*5.5f);
        lcd_draw_line(ax, ay, b1x, b1y, COLOR_YELLOW);
        lcd_draw_line(ax, ay, b2x, b2y, COLOR_YELLOW);
        lcd_draw_line(b1x, b1y, b2x, b2y, COLOR_YELLOW);
    }
}

void action_attitude_simulated(void) {
    const int16_t cx = 160, cy = 120;
    const float PX_PER_DEG = 2.5f;
    const float D2R = (float)M_PI / 180.0f;
    float sim_time = 0.0f;
    bool was_touched = false;

    while (true) {
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched) break;
        was_touched = touched;

        float roll  = 35.0f * sinf(sim_time * 0.35f);
        float pitch = 12.0f * sinf(sim_time * 0.22f);
        sim_time += 0.05f;

        ai_draw_bg(roll * D2R, pitch * PX_PER_DEG);
        ai_draw_pitch_ladder(roll * D2R, pitch);
        ai_draw_bank_arc(roll);

        lcd_draw_line(cx-40, cy,   cx-10, cy,   COLOR_YELLOW);
        lcd_draw_line(cx-40, cy+1, cx-10, cy+1, COLOR_YELLOW);
        lcd_draw_line(cx+10, cy,   cx+40, cy,   COLOR_YELLOW);
        lcd_draw_line(cx+10, cy+1, cx+40, cy+1, COLOR_YELLOW);
        lcd_fill_rect(cx-4, cy-4, 9, 9, COLOR_YELLOW);

        char buf[20];
        lcd_fill_rect(0, 222, 320, 18, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "BNK%+.0f", roll);
        lcd_draw_string(4, 226, buf, COLOR_YELLOW, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "PIT%+.0f", pitch);
        lcd_draw_string(248, 226, buf, COLOR_YELLOW, COLOR_BLACK);

        lcd_flush();
        sleep_ms(50);
    }
}

// ── AHRS attitude indicator (real ICM20948 sensor) ────────────────────────────

void action_test_gyro(void) {
    if (!icm20948_init()) {
        lcd_clear(COLOR_BLACK);
        draw_ribbon_force();
        lcd_draw_string_scaled(20, 60, "SENSOR ERROR", COLOR_RED, COLOR_BLACK, 3);
        lcd_draw_string(20, 110, "WHO_AM_I check failed", COLOR_WHITE, COLOR_BLACK);
        lcd_draw_string(20, 125, "Expected: 0xEA", COLOR_CYAN, COLOR_BLACK);
        lcd_draw_string(20, 200, "TAP: Back", COLOR_WHITE, COLOR_BLACK);
        lcd_flush();
        bool was_touched = false;
        while (true) {
            uint16_t tx, ty;
            bool touched = touch_read(&tx, &ty);
            if (touched && !was_touched) break;
            was_touched = touched;
            sleep_ms(50);
        }
        return;
    }

    MadgwickFilter filter;
    madgwick_init(&filter, 100.0f, 0.05f);
    SensorData accel, gyro;

    // 1-second bias calibration
    float gyro_bias_x = 0, gyro_bias_y = 0, gyro_bias_z = 0;
    float accel_bias_x = 0, accel_bias_y = 0, accel_bias_z = 0;
    const int CAL_N = 100;
    for (int i = 0; i < CAL_N; i++) {
        if (icm20948_read_gyro(&gyro)) {
            gyro_bias_x += icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
            gyro_bias_y += icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
            gyro_bias_z += icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);
        }
        if (icm20948_read_accel(&accel)) {
            accel_bias_x += icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G);
            accel_bias_y += icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G);
            accel_bias_z += icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G);
        }
        sleep_ms(10);
    }
    gyro_bias_x  /= CAL_N; gyro_bias_y  /= CAL_N; gyro_bias_z  /= CAL_N;
    accel_bias_x /= CAL_N; accel_bias_y /= CAL_N; accel_bias_z /= CAL_N;
    accel_bias_z -= 1.0f;

    const int16_t center_x = 160, center_y = 120;
    absolute_time_t last_update = get_absolute_time();
    float display_roll = 0.0f, display_pitch = 0.0f;
    bool display_initialized = false;
    bool was_touched = false;

    while (true) {
        // Touch anywhere → back to menu
        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched) {
            icm20948_sleep();
            break;
        }
        was_touched = touched;

        if (!icm20948_read_accel(&accel) || !icm20948_read_gyro(&gyro))
            continue;

        absolute_time_t now = get_absolute_time();
        int64_t dt_us = absolute_time_diff_us(last_update, now);
        last_update = now;
        float dt = (float)dt_us / 1000000.0f;
        if (dt <= 0.0f || dt > 0.2f) dt = 0.01f;

        float ax = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G) - accel_bias_x;
        float ay = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G) - accel_bias_y;
        float az = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G) - accel_bias_z;

        float gx_raw = icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
        float gy_raw = icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
        float gz_raw = icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);

        float gx_dps = gx_raw - gyro_bias_x;
        float gy_dps = gy_raw - gyro_bias_y;
        float gz_dps = gz_raw - gyro_bias_z;
        if (!isfinite(gx_dps) || !isfinite(gy_dps) || !isfinite(gz_dps)) continue;
        if (fabsf(gx_dps) > 2000.0f || fabsf(gy_dps) > 2000.0f || fabsf(gz_dps) > 2000.0f) continue;

        float acc_norm = sqrtf(ax*ax + ay*ay + az*az);
        bool stationary = isfinite(acc_norm) &&
                          fabsf(acc_norm - 1.0f) < 0.08f &&
                          fabsf(gx_dps) < 1.2f &&
                          fabsf(gy_dps) < 1.2f &&
                          fabsf(gz_dps) < 1.2f;
        if (stationary) {
            float ba = dt * 0.08f; if (ba > 0.02f) ba = 0.02f;
            gyro_bias_x = (1.0f - ba) * gyro_bias_x + ba * gx_raw;
            gyro_bias_y = (1.0f - ba) * gyro_bias_y + ba * gy_raw;
            float bz = dt * 0.25f; if (bz > 0.05f) bz = 0.05f;
            gyro_bias_z = (1.0f - bz) * gyro_bias_z + bz * gz_raw;
            gx_dps = gx_raw - gyro_bias_x;
            gy_dps = gy_raw - gyro_bias_y;
            gz_dps = gz_raw - gyro_bias_z;
        }
        if (fabsf(gz_dps) < 0.15f) gz_dps = 0.0f;

        float gx = gx_dps * M_PI / 180.0f;
        float gy = gy_dps * M_PI / 180.0f;
        float gz = gz_dps * M_PI / 180.0f;

        filter.sample_freq    = 1.0f / dt;
        filter.inv_sample_freq = dt;
        madgwick_update_imu(&filter, gx, gy, gz, ax, ay, az);

        float roll  = -madgwick_get_roll_deg(&filter);
        float pitch =  madgwick_get_pitch_deg(&filter);
        if (!isfinite(roll) || !isfinite(pitch)) {
            madgwick_init(&filter, 100.0f, 0.05f);
            display_initialized = false;
            continue;
        }

        if (!display_initialized) {
            display_roll = roll; display_pitch = pitch;
            display_initialized = true;
        } else {
            float sa = stationary ? 0.08f : 0.25f;
            display_roll  += sa * (roll  - display_roll);
            display_pitch += sa * (pitch - display_pitch);
        }
        if (fabsf(display_roll)  < 0.35f) display_roll  = 0.0f;
        if (fabsf(display_pitch) < 0.35f) display_pitch = 0.0f;
        roll  = display_roll;
        pitch = display_pitch;
        if (pitch >  60.0f) pitch =  60.0f;
        if (pitch < -60.0f) pitch = -60.0f;

        float roll_rad = roll * M_PI / 180.0f;
        float cos_r = cosf(roll_rad);
        float sin_r = sinf(roll_rad);
        float pitch_px = pitch * 2.0f;

        for (int16_t y = 0; y < LCD_HEIGHT; y++) {
            float dy = (float)(y - center_y) + pitch_px;
            if (fabsf(sin_r) < 0.01f) {
                lcd_fill_rect(0, y, LCD_WIDTH, 1, (dy < 0) ? COLOR_SKY : COLOR_BROWN);
            } else {
                int16_t x_cross = center_x - (int16_t)(dy * cos_r / sin_r);
                if (x_cross <= 0) {
                    float test = cos_r * dy + sin_r * (0 - center_x);
                    lcd_fill_rect(0, y, LCD_WIDTH, 1, (test < 0) ? COLOR_SKY : COLOR_BROWN);
                } else if (x_cross >= LCD_WIDTH) {
                    float test = cos_r * dy + sin_r * (0 - center_x);
                    lcd_fill_rect(0, y, LCD_WIDTH, 1, (test < 0) ? COLOR_SKY : COLOR_BROWN);
                } else {
                    float test_left = cos_r * dy + sin_r * (0 - center_x);
                    uint16_t lc = (test_left < 0) ? COLOR_SKY : COLOR_BROWN;
                    uint16_t rc = (test_left < 0) ? COLOR_BROWN : COLOR_SKY;
                    lcd_fill_rect(0,       y, x_cross,             1, lc);
                    lcd_fill_rect(x_cross, y, LCD_WIDTH - x_cross, 1, rc);
                }
            }
        }

        lcd_draw_line(center_x-35, center_y,   center_x-8,  center_y,   COLOR_YELLOW);
        lcd_draw_line(center_x+8,  center_y,   center_x+35, center_y,   COLOR_YELLOW);
        lcd_draw_line(center_x-35, center_y+1, center_x-8,  center_y+1, COLOR_YELLOW);
        lcd_draw_line(center_x+8,  center_y+1, center_x+35, center_y+1, COLOR_YELLOW);
        lcd_fill_rect(center_x-3, center_y-3, 7, 7, COLOR_YELLOW);

        lcd_flush();
    }
}

// ── Menu definition ───────────────────────────────────────────────────────────

#define ICON_COLOR_FLY   0x041F
#define ICON_COLOR_BT    0x601F
#define ICON_COLOR_GYRO  0xFD20
#define ICON_COLOR_RADAR 0x07C0
#define ICON_COLOR_ATT   0xC00F

static MenuItem menu_items[] = {
    {"FLY",      ICON_COLOR_FLY,   action_go_fly},
    {"BLUETOOTH",ICON_COLOR_BT,    action_bluetooth},
    {"AHRS",     ICON_COLOR_GYRO,  action_test_gyro},
    {"RADAR",    ICON_COLOR_RADAR, action_radar},
    {"ATTITUDE", ICON_COLOR_ATT,   action_attitude_simulated}
};

// ── Main ──────────────────────────────────────────────────────────────────────

int main(void) {
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

    touch_init();

    icon_menu_draw(menu_items, ICON_COUNT);

    bool was_touched = false;

    while (true) {
        wifi_poll();

        uint16_t tx, ty;
        bool touched = touch_read(&tx, &ty);
        if (touched && !was_touched) {
            int idx = icon_menu_hit_test(tx, ty);
            if (idx >= 0) {
                icon_menu_flash(idx);
                menu_items[idx].action();
                icon_menu_draw(menu_items, ICON_COUNT);
            }
        }
        was_touched = touched;

        // Refresh WiFi icon in ribbon if state changed
        static uint32_t last_ribbon_ms = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_ribbon_ms >= 200) {
            draw_status_icons();
            last_ribbon_ms = now;
        }

        sleep_ms(10);
    }

    return 0;
}
