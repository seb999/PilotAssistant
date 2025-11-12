/**
 * Pico Command Parser Implementation
 */

#include "pico_commands.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Helper: trim whitespace from string
static void trim_whitespace(char* str) {
    if (!str) return;

    // Trim leading whitespace
    char* start = str;
    while (*start && isspace(*start)) {
        start++;
    }

    // Trim trailing whitespace
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';

    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

bool parse_pico_command(const char* line, PicoCommand* cmd) {
    if (!line || !cmd) {
        return false;
    }

    // Initialize command structure
    memset(cmd, 0, sizeof(PicoCommand));
    cmd->type = CMD_TYPE_NONE;

    // Copy and trim the input line
    strncpy(cmd->raw_string, line, sizeof(cmd->raw_string) - 1);
    cmd->raw_string[sizeof(cmd->raw_string) - 1] = '\0';
    trim_whitespace(cmd->raw_string);

    // Empty line
    if (strlen(cmd->raw_string) == 0) {
        return false;
    }

    // Parse BTN commands: BTN:1,PRESS or BTN:2,RELEASE
    if (strncmp(cmd->raw_string, "BTN:", 4) == 0) {
        cmd->type = CMD_TYPE_BTN;

        // Extract button number and action
        int button_num = 0;
        char action[16] = {0};

        if (sscanf(cmd->raw_string + 4, "%d,%15s", &button_num, action) == 2) {
            snprintf(cmd->display_text, sizeof(cmd->display_text),
                     "BTN:%d,%s", button_num, action);
        } else {
            // Failed to parse, just display raw
            strncpy(cmd->display_text, cmd->raw_string, sizeof(cmd->display_text) - 1);
        }

        return true;
    }

    // Parse JOY commands: JOY:UP, JOY:DOWN, JOY:LEFT, JOY:RIGHT
    if (strncmp(cmd->raw_string, "JOY:", 4) == 0) {
        cmd->type = CMD_TYPE_JOY;
        strncpy(cmd->display_text, cmd->raw_string, sizeof(cmd->display_text) - 1);
        return true;
    }

    // Parse CMD commands: CMD:FLY_MODE, CMD:GYRO_CALIBRATION, CMD:BLUETOOTH
    if (strncmp(cmd->raw_string, "CMD:", 4) == 0) {
        cmd->type = CMD_TYPE_CMD;
        strncpy(cmd->display_text, cmd->raw_string, sizeof(cmd->display_text) - 1);
        return true;
    }

    // Unknown command format
    return false;
}
