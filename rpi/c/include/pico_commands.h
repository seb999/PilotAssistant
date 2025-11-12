/**
 * Pico Command Parser
 * Parses commands received from Pico2 over serial
 */

#ifndef PICO_COMMANDS_H
#define PICO_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>

// Command types
typedef enum {
    CMD_TYPE_NONE = 0,
    CMD_TYPE_BTN,      // Button press/release
    CMD_TYPE_JOY,      // Joystick direction
    CMD_TYPE_CMD       // High-level command
} CommandType;

// Parsed command structure
typedef struct {
    CommandType type;
    char display_text[64];  // Text to display on LCD
    char raw_string[64];    // Original command string
} PicoCommand;

/**
 * Parse a command line from the Pico
 * Returns true if parsing was successful
 *
 * Supported formats:
 *   BTN:1,PRESS
 *   BTN:1,RELEASE
 *   BTN:2,PRESS
 *   BTN:2,RELEASE
 *   BTN:4,PRESS
 *   BTN:5,PRESS (joystick press)
 *   JOY:UP
 *   JOY:DOWN
 *   JOY:LEFT
 *   JOY:RIGHT
 *   CMD:FLY_MODE
 *   CMD:GYRO_CALIBRATION
 *   CMD:BLUETOOTH
 */
bool parse_pico_command(const char* line, PicoCommand* cmd);

#endif // PICO_COMMANDS_H
