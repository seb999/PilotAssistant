#include "menu.h"
#include "pico/stdlib.h"
#include <string.h>

// Row 1: 3 icons  (y=48)  — centers at x=53,160,267
// Row 2: 2 icons  (y=148) — centers at x=107,213
const uint16_t icon_x[ICON_COUNT] = {23, 130, 237, 77, 183};
const uint16_t icon_y[ICON_COUNT] = {48, 48,  48, 148, 148};

// ── Icon symbol drawing (white, inside the 60×60 box) ────────────────────────

static void draw_sym_fly(uint16_t ix, uint16_t iy) {
    // Upward arrow: arrowhead + shaft
    uint16_t cx = ix + 29;
    for (int d = 0; d < 13; d++)
        lcd_fill_rect(cx - d, iy + 9 + d, 2*d + 2, 2, COLOR_WHITE);
    lcd_fill_rect(cx - 5, iy + 34, 12, 15, COLOR_WHITE);
}

static void draw_sym_bluetooth(uint16_t ix, uint16_t iy) {
    // Bluetooth rune: vertical bar + upper-right chevron + lower-left chevron
    uint16_t cx = ix + 30;
    lcd_fill_rect(cx - 1, iy + 10, 3, 40, COLOR_WHITE);
    // Upper chevron (points right)
    lcd_draw_line(cx, iy + 12, cx + 13, iy + 22, COLOR_WHITE);
    lcd_draw_line(cx + 13, iy + 22, cx, iy + 32, COLOR_WHITE);
    // Lower chevron (points left)
    lcd_draw_line(cx, iy + 32, cx - 13, iy + 42, COLOR_WHITE);
    lcd_draw_line(cx - 13, iy + 42, cx, iy + 52, COLOR_WHITE);
}

static void draw_sym_gyro(uint16_t ix, uint16_t iy) {
    // Octagon outline + crosshair — scaled to 34×34 within the icon
    uint16_t bx = ix + 13, by = iy + 13;
    lcd_fill_rect(bx + 8,  by,      18, 3,  COLOR_WHITE);  // top
    lcd_fill_rect(bx + 8,  by + 31, 18, 3,  COLOR_WHITE);  // bottom
    lcd_fill_rect(bx,      by + 8,  3,  18, COLOR_WHITE);  // left
    lcd_fill_rect(bx + 31, by + 8,  3,  18, COLOR_WHITE);  // right
    lcd_fill_rect(bx + 4,  by + 4,  5,  5,  COLOR_WHITE);  // TL corner
    lcd_fill_rect(bx + 25, by + 4,  5,  5,  COLOR_WHITE);  // TR corner
    lcd_fill_rect(bx + 4,  by + 25, 5,  5,  COLOR_WHITE);  // BL corner
    lcd_fill_rect(bx + 25, by + 25, 5,  5,  COLOR_WHITE);  // BR corner
    lcd_fill_rect(bx + 10, by + 15, 14, 4,  COLOR_WHITE);  // horizontal
    lcd_fill_rect(bx + 15, by + 10, 4,  14, COLOR_WHITE);  // vertical
}

static void draw_sym_radar(uint16_t ix, uint16_t iy) {
    uint16_t cx = ix + 30, cy = iy + 34;
    lcd_draw_circle(cx, cy, 8,  COLOR_WHITE);
    lcd_draw_circle(cx, cy, 16, COLOR_WHITE);
    lcd_draw_circle(cx, cy, 22, COLOR_WHITE);
    lcd_draw_line(cx, cy, cx + 17, cy - 14, COLOR_WHITE);  // sweep line
    lcd_fill_rect(cx - 2, cy - 2, 5, 5, COLOR_WHITE);      // center dot
}

static void draw_sym_attitude(uint16_t ix, uint16_t iy) {
    uint16_t cx = ix + 30, cy = iy + 30;
    lcd_draw_circle(cx, cy, 22, COLOR_WHITE);
    lcd_fill_rect(cx - 18, cy - 1, 36, 3, COLOR_WHITE);     // horizon line
    lcd_fill_rect(cx - 13, cy - 2, 9,  3, COLOR_WHITE);     // left wing
    lcd_fill_rect(cx + 5,  cy - 2, 9,  3, COLOR_WHITE);     // right wing
    lcd_fill_rect(cx - 2,  cy - 5, 5,  5, COLOR_WHITE);     // fuselage dot
}

typedef void (*SymDrawFn)(uint16_t, uint16_t);
static const SymDrawFn sym_fns[ICON_COUNT] = {
    draw_sym_fly, draw_sym_bluetooth, draw_sym_gyro,
    draw_sym_radar, draw_sym_attitude
};

// ── Public API ────────────────────────────────────────────────────────────────

static void draw_icon(MenuItem* item, int index) {
    uint16_t ix = icon_x[index], iy = icon_y[index];

    // Colored rounded background
    lcd_fill_round_rect(ix, iy, ICON_SIZE, ICON_SIZE, ICON_RADIUS, item->bg_color);

    // White symbol
    sym_fns[index](ix, iy);

    // Label centered below icon (white on black)
    const char* lbl = item->label;
    int len = strlen(lbl);
    int lx  = (int)ix + (ICON_SIZE - len * 6) / 2;
    if (lx < 0) lx = 0;
    lcd_draw_string(lx, iy + ICON_SIZE + 3, lbl, COLOR_WHITE, COLOR_BLACK);
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
    for (int i = 0; i < ICON_COUNT; i++) {
        if (tx >= icon_x[i] && tx < icon_x[i] + ICON_SIZE &&
            ty >= icon_y[i] && ty < icon_y[i] + ICON_SIZE)
            return i;
    }
    return -1;
}

void icon_menu_flash(int index) {
    if (index < 0 || index >= ICON_COUNT) return;
    uint16_t ix = icon_x[index], iy = icon_y[index];
    // Brief white highlight border
    lcd_fill_round_rect(ix - 3, iy - 3, ICON_SIZE + 6, ICON_SIZE + 6, ICON_RADIUS + 2, COLOR_WHITE);
    lcd_flush_rect(ix - 3, iy - 3, ICON_SIZE + 6, ICON_SIZE + 6);
    sleep_ms(80);
}
