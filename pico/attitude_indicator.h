/**
 * Attitude Indicator for Raspberry Pi Pico 2
 * Hardware: ICM-20948 (9DOF IMU) + ST7789 LCD (320x240)
 * Based on ESP32 attitude indicator with Madgwick AHRS filter
 */

#ifndef ATTITUDE_INDICATOR_H
#define ATTITUDE_INDICATOR_H

#include <stdbool.h>

/**
 * Run the attitude indicator display
 * This is the main entry point - it will initialize everything and run the display loop
 * Call this from your menu action
 */
void attitude_indicator_run(void);

#endif // ATTITUDE_INDICATOR_H
