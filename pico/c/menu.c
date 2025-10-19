#include "menu.h"
#include <stdio.h>
#include <string.h>

// Menu item positions for 320x240 display (similar to MicroPython)
// Each item: x, y, width, height
static const MenuRect default_positions[] = {
    {5, 20, 310, 35},   // Go Fly
    {5, 60, 310, 35},   // Bluetooth
    {5, 100, 310, 35},  // Gyro Offset
    {5, 140, 310, 35}   // Telemetry
};

void menu_init(MenuState* menu, MenuItem* items, int count, MenuRect* positions) {
    menu->items = items;
    menu->item_count = count;
    menu->selection_index = -1;  // No selection initially
    menu->last_selection_index = -2;
    menu->positions = positions ? positions : (MenuRect*)default_positions;
}

// Draw icon: Gyro/calibration
void menu_draw_icon_gyro(uint16_t x, uint16_t y, uint16_t color) {
    // Outer circle (as octagon)
    lcd_fill_rect(x + 4, y, 12, 2, color);      // Top
    lcd_fill_rect(x + 4, y + 18, 12, 2, color);  // Bottom
    lcd_fill_rect(x, y + 4, 2, 12, color);      // Left
    lcd_fill_rect(x + 18, y + 4, 2, 12, color);  // Right
    lcd_fill_rect(x + 2, y + 2, 2, 2, color);   // Corners
    lcd_fill_rect(x + 16, y + 2, 2, 2, color);
    lcd_fill_rect(x + 2, y + 16, 2, 2, color);
    lcd_fill_rect(x + 16, y + 16, 2, 2, color);

    // Center crosshair
    lcd_fill_rect(x + 7, y + 9, 6, 2, color);  // Horizontal
    lcd_fill_rect(x + 9, y + 7, 2, 6, color);  // Vertical
}

// Draw a single menu item
void menu_draw_item(MenuState* menu, int index, bool selected) {
    if (index < 0 || index >= menu->item_count) return;

    MenuRect rect = menu->positions[index];
    const char* label = menu->items[index].label;

    if (selected) {
        // Selected: YELLOW background with BLACK text
        lcd_fill_rect(rect.x, rect.y, rect.width, rect.height, MENU_COLOR_YELLOW);

        // Draw text (left-aligned, vertically centered, scale=3)
        uint16_t text_y = rect.y + 8;  // Approximate vertical center for text
        lcd_draw_string_scaled(rect.x + 10, text_y, label, MENU_COLOR_BLACK, MENU_COLOR_YELLOW, 3);

    } else {
        // Unselected: BLACK background with YELLOW text
        lcd_fill_rect(rect.x, rect.y, rect.width, rect.height, MENU_COLOR_BLACK);

        // Draw text (left-aligned, scale=3)
        uint16_t text_y = rect.y + 8;
        lcd_draw_string_scaled(rect.x + 10, text_y, label, MENU_COLOR_YELLOW, MENU_COLOR_BLACK, 3);
    }
}

// Draw full menu (all items)
void menu_draw_full(MenuState* menu) {
    // Clear screen
    lcd_clear(MENU_COLOR_BLACK);

    // Draw all menu items
    for (int i = 0; i < menu->item_count; i++) {
        menu_draw_item(menu, i, i == menu->selection_index);
    }

    menu->last_selection_index = menu->selection_index;
}

// Update selection (partial redraw)
void menu_update_selection(MenuState* menu) {
    // Only update if selection changed
    if (menu->selection_index == menu->last_selection_index) {
        return;
    }

    // Redraw previously selected item as unselected
    if (menu->last_selection_index >= 0 && menu->last_selection_index < menu->item_count) {
        menu_draw_item(menu, menu->last_selection_index, false);
    }

    // Redraw newly selected item as selected
    if (menu->selection_index >= 0 && menu->selection_index < menu->item_count) {
        menu_draw_item(menu, menu->selection_index, true);
    }

    menu->last_selection_index = menu->selection_index;
}

// Handle input navigation
bool menu_handle_input(MenuState* menu, InputState* input) {
    bool action_selected = false;

    // DOWN - move selection down
    if (input_just_pressed_down(input)) {
        if (menu->selection_index == -1) {
            menu->selection_index = 0;  // Start at first item
        } else {
            menu->selection_index = (menu->selection_index + 1) % menu->item_count;
        }
        menu_update_selection(menu);
        printf("Menu: DOWN - selection=%d\n", menu->selection_index);
    }

    // UP - move selection up
    if (input_just_pressed_up(input)) {
        if (menu->selection_index == -1) {
            menu->selection_index = menu->item_count - 1;  // Start at last item
        } else {
            menu->selection_index = (menu->selection_index - 1 + menu->item_count) % menu->item_count;
        }
        menu_update_selection(menu);
        printf("Menu: UP - selection=%d\n", menu->selection_index);
    }

    // RIGHT or PRESS - execute selected item
    if ((input_just_pressed_right(input) || input_just_pressed_press(input))
        && menu->selection_index >= 0) {
        printf("Menu: SELECTED item %d: %s\n", menu->selection_index,
               menu->items[menu->selection_index].label);

        // Execute action if it exists
        if (menu->items[menu->selection_index].action != NULL) {
            menu->items[menu->selection_index].action();
        }

        action_selected = true;
    }

    return action_selected;
}
