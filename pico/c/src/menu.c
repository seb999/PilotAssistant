#include "menu.h"
#include "pico/stdlib.h"
#include <string.h>
#include <math.h>

// New layout: Large GO FLY button at top, 2 smaller buttons below aligned with GO FLY edges
// Button 0: Large GO FLY (centered at top, x=20, width=280)
// Button 1: BLUETOOTH (left bottom, aligned with left edge of GO FLY)
// Button 2: RADAR (right bottom, aligned with right edge of GO FLY)
const uint16_t icon_x[ICON_COUNT] = {20, 20, 180};  // RADAR at 180 so it ends at 300 (same as GO FLY)
const uint16_t icon_y[ICON_COUNT] = {50, 165, 165};

// ── Icon symbol drawing (white, inside the 60×60 box) ────────────────────────

static void draw_sym_fly(uint16_t ix, uint16_t iy) {
    // Upward arrow: arrowhead + shaft (scaled for 80×80)
    uint16_t cx = ix + 40;
    for (int d = 0; d < 18; d++)
        lcd_fill_rect(cx - d, iy + 12 + d, 2*d + 2, 2, COLOR_WHITE);
    lcd_fill_rect(cx - 7, iy + 45, 16, 20, COLOR_WHITE);
}

static void draw_sym_bluetooth(uint16_t ix, uint16_t iy) {
    // Bluetooth rune: vertical bar + upper-right chevron + lower-left chevron (scaled for 80×80)
    uint16_t cx = ix + 40;
    lcd_fill_rect(cx - 2, iy + 13, 4, 54, COLOR_WHITE);
    // Upper chevron (points right)
    lcd_draw_line(cx, iy + 16, cx + 18, iy + 30, COLOR_WHITE);
    lcd_draw_line(cx + 1, iy + 16, cx + 19, iy + 30, COLOR_WHITE);
    lcd_draw_line(cx + 18, iy + 30, cx, iy + 43, COLOR_WHITE);
    lcd_draw_line(cx + 19, iy + 30, cx + 1, iy + 43, COLOR_WHITE);
    // Lower chevron (points left)
    lcd_draw_line(cx, iy + 43, cx - 18, iy + 57, COLOR_WHITE);
    lcd_draw_line(cx + 1, iy + 43, cx - 17, iy + 57, COLOR_WHITE);
    lcd_draw_line(cx - 18, iy + 57, cx, iy + 70, COLOR_WHITE);
    lcd_draw_line(cx - 17, iy + 57, cx + 1, iy + 70, COLOR_WHITE);
}

static void draw_sym_gyro(uint16_t ix, uint16_t iy) {
    // Octagon outline + crosshair — scaled to 48×48 within the icon
    uint16_t bx = ix + 16, by = iy + 16;
    lcd_fill_rect(bx + 12, by,      24, 4,  COLOR_WHITE);  // top
    lcd_fill_rect(bx + 12, by + 44, 24, 4,  COLOR_WHITE);  // bottom
    lcd_fill_rect(bx,      by + 12, 4,  24, COLOR_WHITE);  // left
    lcd_fill_rect(bx + 44, by + 12, 4,  24, COLOR_WHITE);  // right
    lcd_fill_rect(bx + 5,  by + 5,  7,  7,  COLOR_WHITE);  // TL corner
    lcd_fill_rect(bx + 36, by + 5,  7,  7,  COLOR_WHITE);  // TR corner
    lcd_fill_rect(bx + 5,  by + 36, 7,  7,  COLOR_WHITE);  // BL corner
    lcd_fill_rect(bx + 36, by + 36, 7,  7,  COLOR_WHITE);  // BR corner
    lcd_fill_rect(bx + 14, by + 22, 20, 5,  COLOR_WHITE);  // horizontal
    lcd_fill_rect(bx + 22, by + 14, 5,  20, COLOR_WHITE);  // vertical
}

static void draw_sym_radar(uint16_t ix, uint16_t iy) {
    uint16_t cx = ix + 40, cy = iy + 46;
    lcd_draw_circle(cx, cy, 11, COLOR_WHITE);
    lcd_draw_circle(cx, cy, 22, COLOR_WHITE);
    lcd_draw_circle(cx, cy, 30, COLOR_WHITE);
    lcd_draw_line(cx, cy, cx + 23, cy - 19, COLOR_WHITE);  // sweep line
    lcd_draw_line(cx + 1, cy, cx + 24, cy - 19, COLOR_WHITE);  // thicker sweep
    lcd_fill_rect(cx - 3, cy - 3, 7, 7, COLOR_WHITE);      // center dot
}

typedef void (*SymDrawFn)(uint16_t, uint16_t);
static const SymDrawFn sym_fns[ICON_COUNT] = {
    draw_sym_fly, draw_sym_bluetooth, draw_sym_radar
};

// ── Public API ────────────────────────────────────────────────────────────────

static uint16_t darken_color(uint16_t c) {
    uint16_t r = ((c >> 11) & 0x1F) / 4;
    uint16_t g = ((c >> 5)  & 0x3F) / 4;
    uint16_t b = ( c        & 0x1F) / 4;
    return (r << 11) | (g << 5) | b;
}

// Helper function to create a gradient shade of a color with dithering for smoothness
static uint16_t shade_color(uint16_t base_color, int shade_step, int total_steps) {
    // Extract RGB565 components
    uint16_t r = (base_color >> 11) & 0x1F;
    uint16_t g = (base_color >> 5) & 0x3F;
    uint16_t b = base_color & 0x1F;

    // Calculate brightness factor (0.5 to 1.0 for smoother gradient)
    float factor = 0.5f + (0.5f * shade_step / total_steps);

    // Apply factor with better precision
    float r_float = r * factor;
    float g_float = g * factor;
    float b_float = b * factor;

    // Round to nearest integer for smoother transitions
    r = (uint16_t)(r_float + 0.5f);
    g = (uint16_t)(g_float + 0.5f);
    b = (uint16_t)(b_float + 0.5f);

    // Clamp values
    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;

    return (r << 11) | (g << 5) | b;
}

static void draw_icon(MenuItem* item, int index) {
    uint16_t ix = icon_x[index], iy = icon_y[index];

    if (index == 0) {
        // Large GO FLY button with gradient
        uint16_t btn_w = LARGE_BTN_WIDTH;
        uint16_t btn_h = LARGE_BTN_HEIGHT;

        // Draw gradient background (top to bottom, lighter to darker)
        for (uint16_t y = 0; y < btn_h; y++) {
            uint16_t shade = shade_color(item->bg_color, btn_h - y, btn_h);

            // Draw rounded corners manually for first and last rows
            if (y < 16) {
                // Top rounded corners
                uint16_t corner_offset = 16 - (uint16_t)sqrtf(16*16 - (16-y)*(16-y));
                if (corner_offset > 0) {
                    lcd_fill_rect(ix + corner_offset, iy + y, btn_w - 2*corner_offset, 1, shade);
                } else {
                    lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
                }
            } else if (y >= btn_h - 16) {
                // Bottom rounded corners
                uint16_t y_from_bottom = btn_h - 1 - y;
                uint16_t corner_offset = 16 - (uint16_t)sqrtf(16*16 - (16-y_from_bottom)*(16-y_from_bottom));
                if (corner_offset > 0) {
                    lcd_fill_rect(ix + corner_offset, iy + y, btn_w - 2*corner_offset, 1, shade);
                } else {
                    lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
                }
            } else {
                lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
            }
        }

        lcd_draw_round_rect(ix, iy, btn_w, btn_h, 16, darken_color(item->bg_color));

        // Icon left, label right, both vertically centered
        const char* lbl = item->label;
        int len = strlen(lbl);
        int content_w = 24 + 8 + len * 12;  // icon + gap + 2x text
        int off = (btn_w - content_w) / 2;
        if (off < 8) off = 8;
        lcd_draw_plane_icon(ix + off, iy + (btn_h - 24) / 2, COLOR_WHITE);
        lcd_draw_string_scaled(ix + off + 24 + 8, iy + (btn_h - 14) / 2, lbl, COLOR_WHITE, 0, 2);
    } else {
        // Smaller buttons for BLUETOOTH and RADAR with gradient
        uint16_t btn_w = 120;
        uint16_t btn_h = 60;

        // Draw gradient background (top to bottom, lighter to darker)
        for (uint16_t y = 0; y < btn_h; y++) {
            uint16_t shade = shade_color(item->bg_color, btn_h - y, btn_h);

            // Draw rounded corners manually
            if (y < ICON_RADIUS) {
                // Top rounded corners
                uint16_t corner_offset = ICON_RADIUS - (uint16_t)sqrtf(ICON_RADIUS*ICON_RADIUS - (ICON_RADIUS-y)*(ICON_RADIUS-y));
                if (corner_offset > 0) {
                    lcd_fill_rect(ix + corner_offset, iy + y, btn_w - 2*corner_offset, 1, shade);
                } else {
                    lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
                }
            } else if (y >= btn_h - ICON_RADIUS) {
                // Bottom rounded corners
                uint16_t y_from_bottom = btn_h - 1 - y;
                uint16_t corner_offset = ICON_RADIUS - (uint16_t)sqrtf(ICON_RADIUS*ICON_RADIUS - (ICON_RADIUS-y_from_bottom)*(ICON_RADIUS-y_from_bottom));
                if (corner_offset > 0) {
                    lcd_fill_rect(ix + corner_offset, iy + y, btn_w - 2*corner_offset, 1, shade);
                } else {
                    lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
                }
            } else {
                lcd_fill_rect(ix, iy + y, btn_w, 1, shade);
            }
        }

        lcd_draw_round_rect(ix, iy, btn_w, btn_h, ICON_RADIUS, darken_color(item->bg_color));

        const char* lbl = item->label;
        int len = strlen(lbl);
        if (index == 1 || index == 2) {
            // Icon left, label right, both vertically centered
            int content_w = 24 + 6 + len * 6;
            int off = (btn_w - content_w) / 2;
            if (off < 4) off = 4;
            if (index == 1)
                lcd_draw_bluetooth_icon(ix + off, iy + (btn_h - 24) / 2, false);
            else
                lcd_draw_settings_icon(ix + off, iy + (btn_h - 24) / 2, COLOR_WHITE);
            lcd_draw_string(ix + off + 24 + 6, iy + (btn_h - 7) / 2, lbl, COLOR_WHITE, 0);
        } else {
            int lx = ix + (btn_w - len * 6) / 2;
            int ly = iy + (btn_h - 7) / 2;
            if (lx < ix) lx = ix;
            lcd_draw_string(lx, ly, lbl, COLOR_WHITE, 0);
        }
    }
}

void icon_menu_draw(MenuItem* items, int count) {
    lcd_clear(COLOR_BLACK);

    extern void draw_ribbon_force(void);
    draw_ribbon_force();

    for (int i = 0; i < count && i < ICON_COUNT; i++)
        draw_icon(&items[i], i);

    lcd_flush();
}

int icon_menu_hit_test(uint16_t tx, uint16_t ty) {
    const int PAD = 10;
    for (int i = 0; i < ICON_COUNT; i++) {
        uint16_t btn_w, btn_h;
        if (i == 0) {
            // Large GO FLY button
            btn_w = LARGE_BTN_WIDTH;
            btn_h = LARGE_BTN_HEIGHT;
        } else {
            // Smaller buttons
            btn_w = 120;
            btn_h = 60;
        }

        if ((int)tx >= (int)icon_x[i] - PAD && (int)tx < (int)icon_x[i] + btn_w + PAD &&
            (int)ty >= (int)icon_y[i] - PAD && (int)ty < (int)icon_y[i] + btn_h + PAD)
            return i;
    }
    return -1;
}

void icon_menu_flash(int index) {
    if (index < 0 || index >= ICON_COUNT) return;
    uint16_t ix = icon_x[index], iy = icon_y[index];
    uint16_t btn_w, btn_h, radius;

    if (index == 0) {
        btn_w = LARGE_BTN_WIDTH;
        btn_h = LARGE_BTN_HEIGHT;
        radius = 18;
    } else {
        btn_w = 120;
        btn_h = 60;
        radius = ICON_RADIUS + 2;
    }

    // Brief white highlight border
    lcd_fill_round_rect(ix - 3, iy - 3, btn_w + 6, btn_h + 6, radius, COLOR_WHITE);
    lcd_flush_rect(ix - 3, iy - 3, btn_w + 6, btn_h + 6);
    sleep_ms(80);
}
