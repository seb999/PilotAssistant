#ifndef XPT2046_TOUCH_H
#define XPT2046_TOUCH_H

#include <stdint.h>
#include <stdbool.h>

// Shares SPI1 bus with ST7789 (GP10=SCK, GP11=MOSI, GP12=MISO)
#define TOUCH_CS_PIN  17

// Affine calibration coefficients (computed from 3-corner calibration)
// screen_x = CAL_AX * raw_x + CAL_BX * raw_y + CAL_CX
// screen_y = CAL_AY * raw_x + CAL_BY * raw_y + CAL_CY
#define CAL_AX  (-0.000588f)
#define CAL_BX  ( 0.088505f)
#define CAL_CX  (-27.81f)
#define CAL_AY  (-0.066162f)
#define CAL_BY  (-0.000881f)
#define CAL_CY  (257.34f)

// Initialize touch controller (call after lcd_init)
void touch_init(void);

// Read raw 12-bit ADC values (0–4095)
void touch_read_raw(uint16_t *x_raw, uint16_t *y_raw);

// Read touch position mapped to screen coordinates (0–319, 0–239)
// Returns false if not touched or reading is invalid
bool touch_read(uint16_t *x, uint16_t *y);

#endif // XPT2046_TOUCH_H
