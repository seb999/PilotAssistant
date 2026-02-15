#include "st7789_lcd.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdlib.h>

// SPI instance
#define SPI_PORT spi1
#define SPI_BAUDRATE (62500000)  // 62.5 MHz

// Simple 5x7 font (stored as bits)
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x41, 0x22, 0x14, 0x08, 0x00}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F
    {0x3E, 0x41, 0x41, 0x51, 0x32}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

// Write command to LCD
static void lcd_write_cmd(uint8_t cmd) {
    gpio_put(LCD_DC_PIN, 0);  // Command mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

// Write data to LCD
static void lcd_write_data(uint8_t data) {
    gpio_put(LCD_DC_PIN, 1);  // Data mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

// Write data buffer to LCD
static void lcd_write_data_buffer(const uint8_t* buffer, size_t len) {
    gpio_put(LCD_DC_PIN, 1);  // Data mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD
    spi_write_blocking(SPI_PORT, buffer, len);
    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

// Set address window
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_cmd(0x2A);  // Column address set
    lcd_write_data(x0 >> 8);
    lcd_write_data(x0 & 0xFF);
    lcd_write_data((x1-1) >> 8);
    lcd_write_data((x1-1) & 0xFF);

    lcd_write_cmd(0x2B);  // Row address set
    lcd_write_data(y0 >> 8);
    lcd_write_data(y0 & 0xFF);
    lcd_write_data((y1-1) >> 8);
    lcd_write_data((y1-1) & 0xFF);

    lcd_write_cmd(0x2C);  // Memory write
}

void lcd_init(void) {
    // Initialize SPI
    spi_init(SPI_PORT, SPI_BAUDRATE);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

    // Initialize control pins
    gpio_init(LCD_DC_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);
    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_init(LCD_RST_PIN);
    gpio_set_dir(LCD_RST_PIN, GPIO_OUT);
    gpio_init(LCD_BL_PIN);
    gpio_set_dir(LCD_BL_PIN, GPIO_OUT);

    // Reset display
    sleep_ms(200);  // Wait for power to stabilize
    gpio_put(LCD_RST_PIN, 1);
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, 0);
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, 1);
    sleep_ms(200);

    // Turn on backlight
    gpio_put(LCD_BL_PIN, 1);

    // ST7789 initialization sequence
    // Software reset
    lcd_write_cmd(0x01);
    sleep_ms(150);

    // Sleep out
    lcd_write_cmd(0x11);
    sleep_ms(120);

    // Memory Data Access Control - Row/Col exchange + RGB order + 180° rotation
    lcd_write_cmd(0x36);
    lcd_write_data(0xA0);

    lcd_write_cmd(0x3A);  // Interface Pixel Format
    lcd_write_data(0x05);  // 16-bit color

    lcd_write_cmd(0xB2);  // Porch control
    lcd_write_data(0x0C);
    lcd_write_data(0x0C);
    lcd_write_data(0x00);
    lcd_write_data(0x33);
    lcd_write_data(0x33);

    lcd_write_cmd(0xB7);  // Gate Control
    lcd_write_data(0x35);

    lcd_write_cmd(0xBB);  // VCOM Setting
    lcd_write_data(0x19);

    lcd_write_cmd(0xC0);  // LCM Control
    lcd_write_data(0x2C);

    lcd_write_cmd(0xC2);  // VDV and VRH Command Enable
    lcd_write_data(0x01);

    lcd_write_cmd(0xC3);  // VRH Set
    lcd_write_data(0x12);

    lcd_write_cmd(0xC4);  // VDV Set
    lcd_write_data(0x20);

    lcd_write_cmd(0xC6);  // Frame Rate Control
    lcd_write_data(0x0F);

    lcd_write_cmd(0xD0);  // Power Control 1
    lcd_write_data(0xA4);
    lcd_write_data(0xA1);

    lcd_write_cmd(0xE0);  // Positive Voltage Gamma Control
    lcd_write_data(0xD0);
    lcd_write_data(0x04);
    lcd_write_data(0x0D);
    lcd_write_data(0x11);
    lcd_write_data(0x13);
    lcd_write_data(0x2B);
    lcd_write_data(0x3F);
    lcd_write_data(0x54);
    lcd_write_data(0x4C);
    lcd_write_data(0x18);
    lcd_write_data(0x0D);
    lcd_write_data(0x0B);
    lcd_write_data(0x1F);
    lcd_write_data(0x23);

    lcd_write_cmd(0xE1);  // Negative Voltage Gamma Control
    lcd_write_data(0xD0);
    lcd_write_data(0x04);
    lcd_write_data(0x0C);
    lcd_write_data(0x11);
    lcd_write_data(0x13);
    lcd_write_data(0x2C);
    lcd_write_data(0x3F);
    lcd_write_data(0x44);
    lcd_write_data(0x51);
    lcd_write_data(0x2F);
    lcd_write_data(0x1F);
    lcd_write_data(0x1F);
    lcd_write_data(0x20);
    lcd_write_data(0x23);

    lcd_write_cmd(0x21);  // Display Inversion On

    lcd_write_cmd(0x29);  // Display On
    sleep_ms(20);
}

void lcd_clear(uint16_t color) {
    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t pixel[2] = {hi, lo};

    gpio_put(LCD_DC_PIN, 1);  // Data mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD

    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        spi_write_blocking(SPI_PORT, pixel, 2);
    }

    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    // Clip to screen bounds
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    lcd_set_window(x, y, x + w, y + h);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t pixel[2] = {hi, lo};

    gpio_put(LCD_DC_PIN, 1);  // Data mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD

    for (int i = 0; i < w * h; i++) {
        spi_write_blocking(SPI_PORT, pixel, 2);
    }

    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color) {
    if (ch < 32 || ch > 90) ch = 32;  // Space for unsupported chars

    const uint8_t* glyph = font_5x7[ch - 32];

    for (int j = 0; j < 7; j++) {  // 7 rows
        for (int i = 0; i < 5; i++) {  // 5 columns
            uint16_t pixel_color = (glyph[i] & (1 << j)) ? color : bg_color;
            lcd_set_window(x + i, y + j, x + i + 1, y + j + 1);
            uint8_t pixel[2] = {pixel_color >> 8, pixel_color & 0xFF};
            lcd_write_data_buffer(pixel, 2);
        }
    }
}

void lcd_draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, uint8_t scale) {
    if (ch < 32 || ch > 90) ch = 32;  // Space for unsupported chars
    if (scale < 1) scale = 1;

    const uint8_t* glyph = font_5x7[ch - 32];

    for (int j = 0; j < 7; j++) {  // 7 rows
        for (int i = 0; i < 5; i++) {  // 5 columns
            uint16_t pixel_color = (glyph[i] & (1 << j)) ? color : bg_color;
            // Draw a scaled block for each pixel
            lcd_fill_rect(x + i * scale, y + j * scale, scale, scale, pixel_color);
        }
    }
}

void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    uint16_t orig_x = x;

    while (*str) {
        if (*str == '\n') {
            x = orig_x;
            y += 9;  // Move to next line (7 pixels + 2 spacing)
        } else {
            lcd_draw_char(x, y, *str, color, bg_color);
            x += 6;  // Move to next character (5 pixels + 1 spacing)
        }
        str++;
    }
}

void lcd_draw_string_scaled(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t scale) {
    uint16_t orig_x = x;
    uint8_t char_width = 5 * scale + scale;  // 5 pixels scaled + spacing
    uint8_t char_height = 7 * scale + 2 * scale;  // 7 pixels scaled + spacing

    while (*str) {
        if (*str == '\n') {
            x = orig_x;
            y += char_height;
        } else {
            lcd_draw_char_scaled(x, y, *str, color, bg_color, scale);
            x += char_width;
        }
        str++;
    }
}

void lcd_update(void) {
    // Not needed for direct rendering, but kept for API compatibility
}

void lcd_display_splash(const uint8_t* image_data, size_t data_len) {
    if (!image_data || data_len != (LCD_WIDTH * LCD_HEIGHT * 2)) {
        return;  // Invalid data
    }

    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);

    gpio_put(LCD_DC_PIN, 1);  // Data mode
    gpio_put(LCD_CS_PIN, 0);  // Select LCD

    // Write the entire image buffer
    spi_write_blocking(SPI_PORT, image_data, data_len);

    gpio_put(LCD_CS_PIN, 1);  // Deselect
}

// Draw bitmap with transparency (skips white pixels, can recolor non-white pixels)
void lcd_draw_bitmap_transparent(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                  const uint16_t* bitmap_data, uint16_t replace_color) {
    for (uint16_t py = 0; py < height; py++) {
        for (uint16_t px = 0; px < width; px++) {
            uint16_t color = bitmap_data[py * width + px];

            // Skip white pixels (transparent)
            if (color == COLOR_WHITE) {
                continue;
            }

            // If replace_color is not white, recolor all non-white pixels
            if (replace_color != COLOR_WHITE) {
                color = replace_color;
            }

            // Draw single pixel at position
            lcd_set_window(x + px, y + py, 1, 1);

            uint8_t data[2];
            data[0] = (color >> 8) & 0xFF;
            data[1] = color & 0xFF;

            gpio_put(LCD_DC_PIN, 1);  // Data mode
            gpio_put(LCD_CS_PIN, 0);  // Select LCD
            spi_write_blocking(SPI_PORT, data, 2);
            gpio_put(LCD_CS_PIN, 1);  // Deselect
        }
    }
}

// Draw WiFi icon (signal waves) - Now using 32x32 bitmap
void lcd_draw_wifi_icon(uint16_t x, uint16_t y, bool connected) {
    uint16_t color = connected ? COLOR_GREEN : COLOR_RED;

    #include "img/wifi_icon.h"

    // Draw the bitmap with color replacement
    lcd_draw_bitmap_transparent(x, y, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT,
                                 wifi_icon_data, color);
}

// Draw GPS icon - Now using 32x32 bitmap
void lcd_draw_gps_icon(uint16_t x, uint16_t y, bool connected) {
    uint16_t color = connected ? COLOR_GREEN : COLOR_RED;

    #include "img/gps_icon.h"

    // Draw the bitmap with color replacement
    lcd_draw_bitmap_transparent(x, y, GPS_ICON_WIDTH, GPS_ICON_HEIGHT,
                                 gps_icon_data, color);
}

// Draw Bluetooth icon - Now using 32x32 bitmap
void lcd_draw_bluetooth_icon(uint16_t x, uint16_t y, bool connected) {
    uint16_t color = connected ? COLOR_GREEN : COLOR_RED;

    #include "img/bluetooth_icon.h"

    // Draw the bitmap with color replacement
    lcd_draw_bitmap_transparent(x, y, BLUETOOTH_ICON_WIDTH, BLUETOOTH_ICON_HEIGHT,
                                 bluetooth_icon_data, color);
}

// Draw warning triangle icon - Now using 24x24 bitmap
void lcd_draw_warning_icon(uint16_t x, uint16_t y, bool active) {
    uint16_t color = active ? COLOR_RED : COLOR_AMBER;

    #include "img/warning_icon.h"

    // Draw the bitmap with color replacement
    lcd_draw_bitmap_transparent(x, y, WARNING_ICON_WIDTH, WARNING_ICON_HEIGHT,
                                 warning_icon_data, color);
}

// Draw a single pixel
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    lcd_set_window(x, y, x, y);
    uint8_t data[2];
    data[0] = color >> 8;
    data[1] = color & 0xFF;
    lcd_write_data_buffer(data, 2);
}

// Draw a line using Bresenham's algorithm
void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = abs((int16_t)x1 - (int16_t)x0);
    int16_t dy = abs((int16_t)y1 - (int16_t)y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        lcd_draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Draw a circle using Midpoint Circle Algorithm
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        lcd_draw_pixel(x0 + x, y0 + y, color);
        lcd_draw_pixel(x0 + y, y0 + x, color);
        lcd_draw_pixel(x0 - y, y0 + x, color);
        lcd_draw_pixel(x0 - x, y0 + y, color);
        lcd_draw_pixel(x0 - x, y0 - y, color);
        lcd_draw_pixel(x0 - y, y0 - x, color);
        lcd_draw_pixel(x0 + y, y0 - x, color);
        lcd_draw_pixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}
