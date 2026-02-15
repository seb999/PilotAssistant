#ifndef ST7789_LCD_H
#define ST7789_LCD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Display dimensions (320x240 in landscape mode)
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

// RGB565 color definitions
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_AMBER   0xFD20  // Orange/Amber color (RGB565)

// Pin definitions for Waveshare 1.3" LCD
#define LCD_DC_PIN   8
#define LCD_CS_PIN   9
#define LCD_SCK_PIN  10
#define LCD_MOSI_PIN 11
#define LCD_RST_PIN  12
#define LCD_BL_PIN   13

// Initialize the LCD
void lcd_init(void);

// Clear the screen with a color
void lcd_clear(uint16_t color);

// Draw a filled rectangle
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// Draw a simple text string (5x7 font)
void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color);

// Draw a scaled text string (5x7 font scaled by size)
void lcd_draw_string_scaled(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t scale);

// Draw a single character
void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color);

// Draw a single character scaled
void lcd_draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, uint8_t scale);

// Update the display
void lcd_update(void);

// Display a splash screen from RGB565 buffer
void lcd_display_splash(const uint8_t* image_data, size_t data_len);

// Draw status icons
void lcd_draw_wifi_icon(uint16_t x, uint16_t y, bool connected);
void lcd_draw_gps_icon(uint16_t x, uint16_t y, bool connected);
void lcd_draw_bluetooth_icon(uint16_t x, uint16_t y, bool connected);
void lcd_draw_warning_icon(uint16_t x, uint16_t y, bool active);

// Draw bitmap with transparency (skips COLOR_WHITE pixels, optionally recolors)
void lcd_draw_bitmap_transparent(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                  const uint16_t* bitmap_data, uint16_t replace_color);

// Radar display functions
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

#endif // ST7789_LCD_H
