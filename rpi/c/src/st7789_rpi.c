/**
 * ST7789 LCD Driver for Raspberry Pi
 * Uses Linux spidev and sysfs GPIO
 */

#include "st7789_rpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// Pin definitions (matching Python config)
#define RST_PIN 27
#define DC_PIN  25
#define BL_PIN  24

// SPI configuration
#define SPI_DEVICE   "/dev/spidev0.0"
#define SPI_SPEED_HZ 40000000  // 40 MHz

// GPIO base path
#define GPIO_PATH "/sys/class/gpio"

// Global file descriptors
static int spi_fd = -1;
static int dc_fd = -1;
static int rst_fd = -1;
static int bl_fd = -1;

// Simple 5x7 font
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
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

// Helper: Export GPIO pin
static int gpio_export(int pin) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d", GPIO_PATH, pin);

    struct stat st;
    if (stat(path, &st) == 0) {
        return 0; // Already exported
    }

    int fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, len);
    close(fd);
    usleep(100000);
    return 0;
}

// Helper: Set GPIO direction
static int gpio_set_direction(int pin, const char* direction) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/direction", GPIO_PATH, pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    write(fd, direction, strlen(direction));
    close(fd);
    return 0;
}

// Helper: Open GPIO value file descriptor
static int gpio_open_value(int pin) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/value", GPIO_PATH, pin);
    return open(path, O_WRONLY);
}

// Helper: Set GPIO value
static void gpio_set(int fd, int value) {
    char buf[2] = {value ? '1' : '0', '\0'};
    write(fd, buf, 1);
}

// Helper: Delay
static void delay_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// Write command to LCD
static void lcd_write_cmd(uint8_t cmd) {
    gpio_set(dc_fd, 0);  // Command mode

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)&cmd,
        .len = 1,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = 8,
    };

    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

// Write data to LCD
static void lcd_write_data(uint8_t data) {
    gpio_set(dc_fd, 1);  // Data mode

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)&data,
        .len = 1,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = 8,
    };

    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

// Write data buffer to LCD
static void lcd_write_buffer(const uint8_t* buffer, size_t len) {
    gpio_set(dc_fd, 1);  // Data mode

    const size_t CHUNK_SIZE = 4096;
    size_t offset = 0;

    while (offset < len) {
        size_t chunk_len = (len - offset) > CHUNK_SIZE ? CHUNK_SIZE : (len - offset);

        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)(buffer + offset),
            .len = chunk_len,
            .speed_hz = SPI_SPEED_HZ,
            .bits_per_word = 8,
        };

        ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        offset += chunk_len;
    }
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

int lcd_init(void) {
    // Initialize SPI
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED_HZ;

    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // Initialize GPIO pins
    gpio_export(RST_PIN);
    gpio_export(DC_PIN);
    gpio_export(BL_PIN);

    gpio_set_direction(RST_PIN, "out");
    gpio_set_direction(DC_PIN, "out");
    gpio_set_direction(BL_PIN, "out");

    rst_fd = gpio_open_value(RST_PIN);
    dc_fd = gpio_open_value(DC_PIN);
    bl_fd = gpio_open_value(BL_PIN);

    if (rst_fd < 0 || dc_fd < 0 || bl_fd < 0) {
        fprintf(stderr, "Failed to open GPIO pins\n");
        return -1;
    }

    // Reset display
    gpio_set(rst_fd, 1);
    delay_ms(10);
    gpio_set(rst_fd, 0);
    delay_ms(10);
    gpio_set(rst_fd, 1);
    delay_ms(10);

    // ST7789 initialization sequence
    lcd_write_cmd(0x01);  // Software reset
    delay_ms(150);

    lcd_write_cmd(0x11);  // Sleep out
    delay_ms(255);

    lcd_write_cmd(0x3A);  // Color mode
    lcd_write_data(0x55); // 16-bit RGB565

    lcd_write_cmd(0x36);  // Memory data access control
    lcd_write_data(0x00); // RGB order

    lcd_write_cmd(0x2A);  // Column address set
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data((LCD_WIDTH-1) >> 8);
    lcd_write_data((LCD_WIDTH-1) & 0xFF);

    lcd_write_cmd(0x2B);  // Row address set
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data((LCD_HEIGHT-1) >> 8);
    lcd_write_data((LCD_HEIGHT-1) & 0xFF);

    lcd_write_cmd(0x21);  // Inversion on
    lcd_write_cmd(0x13);  // Normal display on
    lcd_write_cmd(0x29);  // Display on
    delay_ms(100);

    // Turn on backlight
    gpio_set(bl_fd, 1);

    return 0;
}

void lcd_cleanup(void) {
    if (bl_fd >= 0) {
        gpio_set(bl_fd, 0);
        close(bl_fd);
    }
    if (rst_fd >= 0) close(rst_fd);
    if (dc_fd >= 0) close(dc_fd);
    if (spi_fd >= 0) close(spi_fd);
}

void lcd_clear(uint16_t color) {
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    lcd_set_window(x, y, x + w, y + h);

    uint8_t color_bytes[2] = {color >> 8, color & 0xFF};
    uint32_t total_pixels = (uint32_t)w * h;

    for (uint32_t i = 0; i < total_pixels; i++) {
        lcd_write_buffer(color_bytes, 2);
    }
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    lcd_set_window(x, y, x + 1, y + 1);
    uint8_t color_bytes[2] = {color >> 8, color & 0xFF};
    lcd_write_buffer(color_bytes, 2);
}

void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int16_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
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

void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color) {
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;

    lcd_draw_pixel(x0, y0 + radius, color);
    lcd_draw_pixel(x0, y0 - radius, color);
    lcd_draw_pixel(x0 + radius, y0, color);
    lcd_draw_pixel(x0 - radius, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        lcd_draw_pixel(x0 + x, y0 + y, color);
        lcd_draw_pixel(x0 - x, y0 + y, color);
        lcd_draw_pixel(x0 + x, y0 - y, color);
        lcd_draw_pixel(x0 - x, y0 - y, color);
        lcd_draw_pixel(x0 + y, y0 + x, color);
        lcd_draw_pixel(x0 - y, y0 + x, color);
        lcd_draw_pixel(x0 + y, y0 - x, color);
        lcd_draw_pixel(x0 - y, y0 - x, color);
    }
}

void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color) {
    if (ch < 32 || ch > 90) ch = 32;

    const uint8_t* glyph = font_5x7[ch - 32];

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 8; row++) {
            if (line & 0x01) {
                lcd_draw_pixel(x + col, y + row, color);
            } else {
                lcd_draw_pixel(x + col, y + row, bg_color);
            }
            line >>= 1;
        }
    }
}

void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    uint16_t offset = 0;
    while (*str) {
        lcd_draw_char(x + offset, y, *str, color, bg_color);
        offset += 6;
        str++;
    }
}

void lcd_draw_string_scaled(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t scale) {
    if (scale == 0) scale = 1;
    uint16_t offset = 0;

    while (*str) {
        if (*str < 32 || *str > 90) {
            str++;
            continue;
        }

        const uint8_t* glyph = font_5x7[*str - 32];

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t line = glyph[col];
            for (uint8_t row = 0; row < 8; row++) {
                uint16_t pixel_color = (line & 0x01) ? color : bg_color;

                for (uint8_t sx = 0; sx < scale; sx++) {
                    for (uint8_t sy = 0; sy < scale; sy++) {
                        lcd_draw_pixel(x + offset + col * scale + sx, y + row * scale + sy, pixel_color);
                    }
                }
                line >>= 1;
            }
        }
        offset += 6 * scale;
        str++;
    }
}
