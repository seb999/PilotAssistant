// 32x32 RGB565 Bluetooth icon (bold) — 0xFFFF = transparent
#ifndef BT_BUTTON_ICON_H
#define BT_BUTTON_ICON_H

#include <stdint.h>

#define BT_BTN_ICON_W 32
#define BT_BTN_ICON_H 32

#define T 0xFFFF
#define B 0x001F

const uint16_t bt_button_icon_data[32 * 32] = {
// Row 0-6: top padding
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
// Row 5-9: upper diagonals
T,T,T,T,T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,B,B,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,
// Row 10-12: center vertical bar
T,T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
// Row 13-17: middle crossing diagonals
T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,B,B,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
// Row 18-21: lower diagonals
T,T,T,T,T,T,T,T,T,T,T,T,B,B,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,B,B,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,B,B,T,T,T,T,T,T,B,B,T,T,T,T,T,T,T,T,T,T,T,T,T,
// Row 22-31: bottom padding
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,T,
};

#undef T
#undef B

#endif // BT_BUTTON_ICON_H
