#include <stdio.h>
#include "pico/stdlib.h"
#include "st7789_lcd.h"
#include "xpt2046_touch.h"

static void draw_cross(uint16_t x, uint16_t y, uint16_t color) {
    lcd_draw_line(x - 10, y, x + 10, y, color);
    lcd_draw_line(x, y - 10, x, y + 10, color);
    lcd_draw_circle(x, y, 6, color);
}

int main(void) {
    stdio_init_all();
    sleep_ms(1000);

    lcd_init();
    touch_init();

    printf("Touch test starting\n");
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(30, 100, "Touch test", COLOR_CYAN,  COLOR_BLACK, 2);
    lcd_draw_string       (80, 130, "Touch anywhere", COLOR_WHITE, COLOR_BLACK);
    lcd_flush();
    sleep_ms(1500);

    uint16_t last_x = 0xFFFF, last_y = 0xFFFF;

    while (true) {
        uint16_t sx, sy;

        if (touch_read(&sx, &sy)) {
            printf("X:%d Y:%d\n", sx, sy);
          int dx = (int)sx - (int)last_x;
            int dy = (int)sy - (int)last_y;

            if (last_x == 0xFFFF || (dx*dx + dy*dy) > 16) {
                if (last_x != 0xFFFF)
                    lcd_fill_rect(last_x > 12 ? last_x-12 : 0,
                                  last_y > 12 ? last_y-12 : 0, 25, 25, COLOR_BLACK);

                draw_cross(sx, sy, COLOR_GREEN);

                char buf[32];
                lcd_fill_rect(0, 0, 210, 16, COLOR_BLACK);
                snprintf(buf, sizeof(buf), "X:%3d Y:%3d", sx, sy);
                lcd_draw_string(2, 2, buf, COLOR_YELLOW, COLOR_BLACK);
                lcd_flush();

                last_x = sx;
                last_y = sy;
            }
        } else if (last_x != 0xFFFF) {
            lcd_fill_rect(last_x > 12 ? last_x-12 : 0,
                          last_y > 12 ? last_y-12 : 0, 25, 25, COLOR_BLACK);
            lcd_flush();
            last_x = 0xFFFF;
            last_y = 0xFFFF;
        }

        sleep_ms(5);
    }
}
