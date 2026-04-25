#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "st7789_lcd.h"

#define ICON_COUNT  5
#define ICON_SIZE   60
#define ICON_RADIUS 10

typedef struct {
    const char* label;
    uint16_t    bg_color;
    void (*action)(void);
} MenuItem;

// Icon grid top-left positions (x, y): row1=3 icons, row2=2 centered
extern const uint16_t icon_x[ICON_COUNT];
extern const uint16_t icon_y[ICON_COUNT];

void icon_menu_draw(MenuItem* items, int count);
int  icon_menu_hit_test(uint16_t tx, uint16_t ty);
void icon_menu_flash(int index);

#endif // MENU_H
