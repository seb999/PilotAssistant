/**
 * ST7789 LCD Driver for Raspberry Pi
 * 320x240 display via SPI (landscape mode for HUD)
 */

#ifndef ST7789_RPI_H
#define ST7789_RPI_H

#include <stdint.h>
#include <stdbool.h>

// Display dimensions (landscape mode)
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

/**
 * Initialize the LCD
 */
int lcd_init(void);

/**
 * Clean up and close LCD
 */
void lcd_cleanup(void);

/**
 * Clear the screen with a color
 */
void lcd_clear(uint16_t color);

/**
 * Fill a rectangular region
 */
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * Draw a single pixel
 */
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * Draw a line
 */
void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/**
 * Draw a circle
 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

/**
 * Draw a character (5x7 font)
 */
void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color);

/**
 * Draw a string
 */
void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color);

/**
 * Draw a scaled string
 */
void lcd_draw_string_scaled(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t scale);

/**
 * Draw an RGB565 image buffer
 */
void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* image_data);

/**
 * Load and display a PNG image from file
 * Returns 0 on success, -1 on error
 */
int lcd_display_png(const char* filename);

#endif // ST7789_RPI_H
