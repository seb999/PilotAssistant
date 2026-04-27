#include "xpt2046_touch.h"
#include "st7789_lcd.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define SPI_PORT       spi1
#define SPI_TOUCH_BAUD 1000000
#define SPI_LCD_BAUD   40000000

#define CMD_READ_X   0xD0
#define CMD_READ_Y   0x90
#define CMD_READ_Z1  0xB0
#define Z_THRESHOLD  100

#define NUM_SAMPLES  8
#define TRIM         2

static int     lift_count = 0;

static void touch_spi_begin(void) {
    // Drain any stale bytes left in the RX FIFO from LCD DMA operations.
    // The LCD flush only writes (DMA TX), so MISO samples accumulate in the RX
    // FIFO unchecked. Reading those stale 0xFF bytes instead of real XPT2046
    // data causes z1 to read as 4095.
    while (spi_is_readable(SPI_PORT))
        (void)spi_get_hw(SPI_PORT)->dr;

    spi_set_baudrate(SPI_PORT, SPI_TOUCH_BAUD);
    gpio_put(TOUCH_CS_PIN, 0);
    sleep_us(50);
}

static void touch_spi_end(void) {
    gpio_put(TOUCH_CS_PIN, 1);
    spi_set_baudrate(SPI_PORT, SPI_LCD_BAUD);
}

static uint16_t touch_read_channel(uint8_t cmd) {
    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3];

    // Dummy read: XPT2046 returns stale data on the first conversion after
    // the command byte — discard it and re-read to get a settled value.
    spi_write_read_blocking(SPI_PORT, tx, rx, 3);
    spi_write_read_blocking(SPI_PORT, tx, rx, 3);

    return ((rx[1] & 0x7F) << 5) | (rx[2] >> 3);
}

static uint16_t sample_channel(uint8_t cmd) {
    uint16_t buf[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; i++)
        buf[i] = touch_read_channel(cmd);
    for (int i = 1; i < NUM_SAMPLES; i++) {
        uint16_t key = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j] > key) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = key;
    }
    uint32_t sum = 0;
    for (int i = TRIM; i < NUM_SAMPLES - TRIM; i++)
        sum += buf[i];
    return sum / (NUM_SAMPLES - 2 * TRIM);
}

void touch_init(void) {
    gpio_init(TOUCH_CS_PIN);
    gpio_set_dir(TOUCH_CS_PIN, GPIO_OUT);
    gpio_put(TOUCH_CS_PIN, 1);
}

void touch_read_raw(uint16_t *x_raw, uint16_t *y_raw) {
    touch_spi_begin();
    *x_raw = sample_channel(CMD_READ_X);
    sleep_us(10);
    *y_raw = sample_channel(CMD_READ_Y);
    touch_spi_end();
}

bool touch_read(uint16_t *x, uint16_t *y) {
    touch_spi_begin();

    uint16_t z1 = touch_read_channel(CMD_READ_Z1);

    if (z1 < Z_THRESHOLD) {
        touch_spi_end();
        lift_count = 0;
        return false;
    }
    lift_count = 0;

    sleep_us(10);
    uint16_t x_raw = sample_channel(CMD_READ_X);
    sleep_us(10);
    uint16_t y_raw = sample_channel(CMD_READ_Y);
    touch_spi_end();

    // Affine transform — maps raw ADC to screen coords directly
    int32_t sx = (int32_t)(CAL_AX * x_raw + CAL_BX * y_raw + CAL_CX);
    int32_t sy = (int32_t)(CAL_AY * x_raw + CAL_BY * y_raw + CAL_CY);

    if (sx < 0) sx = 0;
    if (sx >= LCD_WIDTH)  sx = LCD_WIDTH  - 1;
    if (sy < 0) sy = 0;
    if (sy >= LCD_HEIGHT) sy = LCD_HEIGHT - 1;

    *x = (uint16_t)sx;
    *y = (uint16_t)sy;
    return true;
}
