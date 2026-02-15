/**
 * ST7789 LCD Driver for Raspberry Pi
 * Uses Linux spidev and libgpiod
 */

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include "../include/st7789_rpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <gpiod.h>

// Pin definitions (matching Python config)
#define RST_PIN 27
#define DC_PIN  25
#define BL_PIN  24

// SPI configuration
#define SPI_DEVICE   "/dev/spidev0.0"
#define SPI_SPEED_HZ 40000000  // 40 MHz (stable speed)

// GPIO chip
#define GPIO_CHIP "gpiochip0"

// Global handles
static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line *dc_line = NULL;
static struct gpiod_line *rst_line = NULL;
static struct gpiod_line *bl_line = NULL;

// Framebuffer for double buffering
static uint16_t *framebuffer = NULL;

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

// Helper: Set GPIO value
static void gpio_set(struct gpiod_line *line, int value) {
    if (line) {
        gpiod_line_set_value(line, value);
    }
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
    gpio_set(dc_line, 0);  // Command mode

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
    gpio_set(dc_line, 1);  // Data mode

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
    gpio_set(dc_line, 1);  // Data mode

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

    // Initialize GPIO using libgpiod
    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        fprintf(stderr, "Failed to open GPIO chip\n");
        close(spi_fd);
        return -1;
    }

    // Request GPIO lines as outputs
    rst_line = gpiod_chip_get_line(chip, RST_PIN);
    dc_line = gpiod_chip_get_line(chip, DC_PIN);
    bl_line = gpiod_chip_get_line(chip, BL_PIN);

    if (!rst_line || !dc_line || !bl_line) {
        fprintf(stderr, "Failed to get GPIO lines\n");
        gpiod_chip_close(chip);
        close(spi_fd);
        return -1;
    }

    if (gpiod_line_request_output(rst_line, "st7789-rst", 0) < 0 ||
        gpiod_line_request_output(dc_line, "st7789-dc", 0) < 0 ||
        gpiod_line_request_output(bl_line, "st7789-bl", 0) < 0) {
        fprintf(stderr, "Failed to request GPIO lines as outputs\n");
        gpiod_chip_close(chip);
        close(spi_fd);
        return -1;
    }

    // Reset display
    gpio_set(rst_line, 1);
    delay_ms(10);
    gpio_set(rst_line, 0);
    delay_ms(10);
    gpio_set(rst_line, 1);
    delay_ms(10);

    // ST7789 initialization sequence
    lcd_write_cmd(0x01);  // Software reset
    delay_ms(150);

    lcd_write_cmd(0x11);  // Sleep out
    delay_ms(255);

    lcd_write_cmd(0x3A);  // Color mode
    lcd_write_data(0x55); // 16-bit RGB565

    lcd_write_cmd(0x36);  // Memory data access control
    lcd_write_data(0x70); // Landscape mode (MV=1, MX=1, MY=1) for 320x240

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
    gpio_set(bl_line, 1);

    // Allocate framebuffer (RGB565 = 2 bytes per pixel)
    framebuffer = (uint16_t*)malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    if (!framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer\n");
        lcd_cleanup();
        return -1;
    }

    // Clear framebuffer to black
    memset(framebuffer, 0, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));

    return 0;
}

void lcd_cleanup(void) {
    // Free framebuffer
    if (framebuffer) {
        free(framebuffer);
        framebuffer = NULL;
    }

    if (bl_line) {
        gpio_set(bl_line, 0);
        gpiod_line_release(bl_line);
    }
    if (rst_line) gpiod_line_release(rst_line);
    if (dc_line) gpiod_line_release(dc_line);
    if (chip) gpiod_chip_close(chip);
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

// Convert RGB888 to RGB565
static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* image_data) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    lcd_set_window(x, y, x + w, y + h);

    // Send image data in chunks
    const size_t CHUNK_PIXELS = 2048;
    uint8_t buffer[CHUNK_PIXELS * 2];
    uint32_t total_pixels = (uint32_t)w * h;
    uint32_t pixels_sent = 0;

    while (pixels_sent < total_pixels) {
        uint32_t chunk_pixels = (total_pixels - pixels_sent) > CHUNK_PIXELS ?
                                CHUNK_PIXELS : (total_pixels - pixels_sent);

        // Convert RGB565 to bytes (big endian)
        for (uint32_t i = 0; i < chunk_pixels; i++) {
            uint16_t pixel = image_data[pixels_sent + i];
            buffer[i * 2] = pixel >> 8;
            buffer[i * 2 + 1] = pixel & 0xFF;
        }

        lcd_write_buffer(buffer, chunk_pixels * 2);
        pixels_sent += chunk_pixels;
    }
}

int lcd_display_png(const char* filename) {
    int width, height, channels;

    // Load the image
    unsigned char* img = stbi_load(filename, &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", filename);
        return -1;
    }

    printf("Loaded image: %dx%d, channels=%d\n", width, height, channels);

    // Scale/center the image to fit the display
    uint16_t display_w = width > LCD_WIDTH ? LCD_WIDTH : width;
    uint16_t display_h = height > LCD_HEIGHT ? LCD_HEIGHT : height;
    uint16_t offset_x = (LCD_WIDTH - display_w) / 2;
    uint16_t offset_y = (LCD_HEIGHT - display_h) / 2;

    // Allocate buffer for RGB565 data
    uint16_t* rgb565_buffer = (uint16_t*)malloc(display_w * display_h * sizeof(uint16_t));
    if (!rgb565_buffer) {
        stbi_image_free(img);
        return -1;
    }

    // Convert RGB888 to RGB565 with cyan tint
    for (int dy = 0; dy < display_h; dy++) {
        for (int dx = 0; dx < display_w; dx++) {
            int src_y = (dy * height) / display_h;
            int src_x = (dx * width) / display_w;
            int src_idx = (src_y * width + src_x) * 3;

            uint8_t r = img[src_idx];
            uint8_t g = img[src_idx + 1];
            uint8_t b = img[src_idx + 2];

            // Convert to grayscale
            uint8_t gray = (r * 30 + g * 59 + b * 11) / 100;

            // Apply cyan tint (0 red, full green and blue)
            uint8_t cyan_r = 0;
            uint8_t cyan_g = gray;
            uint8_t cyan_b = gray;

            rgb565_buffer[dy * display_w + dx] = rgb888_to_rgb565(cyan_r, cyan_g, cyan_b);
        }
    }

    // Display the image
    lcd_draw_image(offset_x, offset_y, display_w, display_h, rgb565_buffer);

    // Cleanup
    free(rgb565_buffer);
    stbi_image_free(img);

    return 0;
}

// ============================================================================
// FRAMEBUFFER FUNCTIONS (Double Buffering)
// ============================================================================

uint16_t* lcd_get_framebuffer(void) {
    return framebuffer;
}

void lcd_display_framebuffer(void) {
    if (!framebuffer) return;

    // Set window to full screen
    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);

    // Optimized: Use larger chunks and loop unrolling for conversion
    const size_t CHUNK_PIXELS = 16384;  // 32KB chunks (16384 pixels * 2 bytes)
    uint8_t buffer[CHUNK_PIXELS * 2];
    uint32_t total_pixels = LCD_WIDTH * LCD_HEIGHT;
    uint32_t pixels_sent = 0;

    while (pixels_sent < total_pixels) {
        uint32_t chunk_pixels = (total_pixels - pixels_sent) > CHUNK_PIXELS ?
                                CHUNK_PIXELS : (total_pixels - pixels_sent);

        // Fast conversion using pointer arithmetic and loop unrolling
        uint16_t *src = framebuffer + pixels_sent;
        uint8_t *dst = buffer;

        // Process 4 pixels at a time for better CPU cache usage
        uint32_t i;
        for (i = 0; i + 3 < chunk_pixels; i += 4) {
            uint16_t p0 = src[0];
            uint16_t p1 = src[1];
            uint16_t p2 = src[2];
            uint16_t p3 = src[3];

            dst[0] = p0 >> 8;
            dst[1] = p0 & 0xFF;
            dst[2] = p1 >> 8;
            dst[3] = p1 & 0xFF;
            dst[4] = p2 >> 8;
            dst[5] = p2 & 0xFF;
            dst[6] = p3 >> 8;
            dst[7] = p3 & 0xFF;

            src += 4;
            dst += 8;
        }

        // Handle remaining pixels
        for (; i < chunk_pixels; i++) {
            uint16_t pixel = *src++;
            *dst++ = pixel >> 8;
            *dst++ = pixel & 0xFF;
        }

        // Use lcd_write_buffer which properly handles DC line and SPI
        lcd_write_buffer(buffer, chunk_pixels * 2);
        pixels_sent += chunk_pixels;
    }
}

void lcd_fb_clear(uint16_t color) {
    if (!framebuffer) return;

    // Fast clear using optimized approach
    if (color == 0x0000) {
        // Black - use memset (fastest)
        memset(framebuffer, 0, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    } else {
        // Other colors - use optimized loop with unrolling
        uint32_t total = LCD_WIDTH * LCD_HEIGHT;
        uint16_t *fb = framebuffer;

        // Process 8 pixels at a time
        uint32_t i;
        for (i = 0; i + 7 < total; i += 8) {
            fb[0] = color;
            fb[1] = color;
            fb[2] = color;
            fb[3] = color;
            fb[4] = color;
            fb[5] = color;
            fb[6] = color;
            fb[7] = color;
            fb += 8;
        }

        // Handle remaining pixels
        for (; i < total; i++) {
            *fb++ = color;
        }
    }
}

void lcd_fb_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (!framebuffer || x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    framebuffer[y * LCD_WIDTH + x] = color;
}

void lcd_fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!framebuffer) return;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    // Clip to screen bounds
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    // Optimized: fill row by row with pointer arithmetic
    for (uint16_t dy = 0; dy < h; dy++) {
        uint16_t *row = framebuffer + (y + dy) * LCD_WIDTH + x;

        // Fill row using unrolled loop for speed
        uint16_t dx;
        for (dx = 0; dx + 7 < w; dx += 8) {
            row[0] = color;
            row[1] = color;
            row[2] = color;
            row[3] = color;
            row[4] = color;
            row[5] = color;
            row[6] = color;
            row[7] = color;
            row += 8;
        }

        // Handle remaining pixels
        for (; dx < w; dx++) {
            *row++ = color;
        }
    }
}

void lcd_fb_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    if (!framebuffer) return;

    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        lcd_fb_draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
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

void lcd_fb_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    if (!framebuffer || !str) return;

    while (*str) {
        // Draw character background
        for (int dy = 0; dy < 8; dy++) {
            for (int dx = 0; dx < 6; dx++) {
                lcd_fb_draw_pixel(x + dx, y + dy, bg_color);
            }
        }

        // Draw character
        if (*str >= 32 && *str <= 'Z') {
            const uint8_t *glyph = font_5x7[*str - 32];
            for (int col = 0; col < 5; col++) {
                uint8_t line = glyph[col];
                for (int row = 0; row < 7; row++) {
                    if (line & (1 << row)) {
                        lcd_fb_draw_pixel(x + col, y + row, color);
                    }
                }
            }
        }

        x += 6;
        str++;
    }
}
