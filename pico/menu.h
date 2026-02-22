#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "st7789_lcd.h"
#include "input_handler.h"

// Menu colors (RGB565)
#define MENU_COLOR_BLACK   0x0000
#define MENU_COLOR_WHITE   0xFFFF
#define MENU_COLOR_ORANGE  0x07FF  // Cyan color (matching GYRO OFFSET title)
#define MENU_COLOR_CYAN    0x07FF
#define MENU_COLOR_MAGENTA 0xF81F

// Menu item rectangle positions (x, y, width, height)
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} MenuRect;

// Menu item structure
typedef struct {
    const char* label;
    void (*action)(void);  // Function to call when selected
} MenuItem;

// Menu state
typedef struct {
    MenuItem* items;
    int item_count;
    int selection_index;
    int last_selection_index;
    MenuRect* positions;
} MenuState;

// Menu initialization
void menu_init(MenuState* menu, MenuItem* items, int count, MenuRect* positions);

// Draw the full menu (initial display)
void menu_draw_full(MenuState* menu);

// Update menu display (partial update for selection changes)
void menu_update_selection(MenuState* menu);

// Draw a single menu item
void menu_draw_item(MenuState* menu, int index, bool selected);

// Handle navigation input and return true if action was selected
bool menu_handle_input(MenuState* menu, InputState* input);

// Icon drawing functions (24x20 pixels)
void menu_draw_icon_go_fly(uint16_t x, uint16_t y, uint16_t color);
void menu_draw_icon_bluetooth(uint16_t x, uint16_t y, uint16_t color);
void menu_draw_icon_gyro(uint16_t x, uint16_t y, uint16_t color);

#endif // MENU_H
