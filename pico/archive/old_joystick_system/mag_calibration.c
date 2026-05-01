/**
 * Magnetometer Calibration for ICM-20948
 * Stores hard-iron offsets in flash memory to persist across reboots
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "mag_calibration.h"
#include "icm20948_sensor.h"
#include "st7789_lcd.h"
#include "input_handler.h"

// Magic number to identify valid calibration data
#define MAG_CAL_MAGIC 0x4D414743  // "MAGC"

// Flash offset for calibration data (last sector of flash)
// Pico 2 has 16MB flash, we use the last 4KB sector
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Compile-time check to ensure calibration data fits in a flash page
_Static_assert(sizeof(MagCalibration) <= FLASH_PAGE_SIZE,
               "MagCalibration struct is too large for flash page");

// Global calibration data (loaded at startup)
static MagCalibration current_calibration = {0};
static bool calibration_loaded = false;

/**
 * Load magnetometer calibration from flash
 */
bool mag_calibration_load(MagCalibration* cal) {
    printf("Flash size: %lu bytes, offset: 0x%lx\n",
           (unsigned long)PICO_FLASH_SIZE_BYTES, (unsigned long)FLASH_TARGET_OFFSET);

    // Read from flash
    const MagCalibration* flash_cal = (const MagCalibration*)(XIP_BASE + FLASH_TARGET_OFFSET);

    // Verify magic number (now first field in struct)
    if (flash_cal->magic == MAG_CAL_MAGIC) {
        memcpy(cal, flash_cal, sizeof(MagCalibration));
        memcpy(&current_calibration, flash_cal, sizeof(MagCalibration));
        calibration_loaded = true;
        printf("Mag calibration loaded: offset_x=%.2f, offset_y=%.2f, offset_z=%.2f\n",
               cal->offset_x, cal->offset_y, cal->offset_z);
        return true;
    }

    // No valid calibration found
    printf("No valid mag calibration found in flash (magic=0x%08lx, expected=0x%08lx)\n",
           (unsigned long)flash_cal->magic, (unsigned long)MAG_CAL_MAGIC);
    cal->magic = 0;
    cal->offset_x = 0.0f;
    cal->offset_y = 0.0f;
    cal->offset_z = 0.0f;
    calibration_loaded = false;
    return false;
}

/**
 * Save magnetometer calibration to flash
 * MUST run from RAM since we're modifying flash!
 */
bool __not_in_flash_func(mag_calibration_save)(const MagCalibration* cal) {
    printf("Saving calibration to flash at offset 0x%lx\n", (unsigned long)FLASH_TARGET_OFFSET);

    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, cal, sizeof(MagCalibration));

    // Disable interrupts during flash write
    uint32_t ints = save_and_disable_interrupts();

    // Erase sector
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write page
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);

    restore_interrupts(ints);

    // Flush XIP cache to ensure we read fresh data
    extern void flash_flush_cache(void);
    flash_flush_cache();

    printf("Flash write complete, verifying...\n");

    // Verify the write by reading back
    const MagCalibration* flash_cal = (const MagCalibration*)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (flash_cal->magic != cal->magic ||
        flash_cal->offset_x != cal->offset_x ||
        flash_cal->offset_y != cal->offset_y ||
        flash_cal->offset_z != cal->offset_z) {
        printf("ERROR: Flash verification failed!\n");
        printf("  Expected magic: 0x%08lx, got: 0x%08lx\n",
               (unsigned long)cal->magic, (unsigned long)flash_cal->magic);
        return false;
    }

    printf("Mag calibration saved and verified in flash\n");

    // Update current calibration
    memcpy(&current_calibration, cal, sizeof(MagCalibration));
    calibration_loaded = true;

    return true;
}

/**
 * Apply calibration offsets to raw magnetometer readings
 */
void mag_calibration_apply(float* mx, float* my, float* mz) {
    if (calibration_loaded) {
        *mx -= current_calibration.offset_x;
        *my -= current_calibration.offset_y;
        *mz -= current_calibration.offset_z;
    }
}

/**
 * Run magnetometer calibration routine
 */
void mag_calibration_run(void) {
    printf("=== Magnetometer Calibration ===\n");

    // Show initialization message
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(60, 100, "STARTING", COLOR_WHITE, COLOR_BLACK, 2);
    lcd_flush();

    // Test if sensor is already initialized by attempting a read
    SensorData test_accel;
    bool sensor_initialized = icm20948_read_accel(&test_accel);

    if (!sensor_initialized) {
        printf("Sensors not initialized, initializing now...\n");

        if (!icm20948_init()) {
            printf("ERROR: Failed to initialize ICM-20948\n");
            lcd_clear(COLOR_BLACK);
            lcd_draw_string_scaled(20, 100, "SENSOR INIT FAILED", COLOR_RED, COLOR_BLACK, 2);
            lcd_flush();
            sleep_ms(3000);
            return;
        }

        if (!icm20948_init_magnetometer()) {
            printf("ERROR: Failed to initialize magnetometer\n");
            lcd_clear(COLOR_BLACK);
            lcd_draw_string_scaled(20, 100, "MAG INIT FAILED", COLOR_RED, COLOR_BLACK, 2);
            lcd_flush();
            sleep_ms(3000);
            return;
        }

        printf("Sensors initialized, waiting for stabilization...\n");
        sleep_ms(1000);  // Give sensors time to stabilize after init
    } else {
        printf("Using already-initialized sensors\n");

        // Flush any stale magnetometer data by reading and discarding
        // This prevents persistent data overrun issues
        printf("Flushing stale magnetometer data...\n");
        SensorData flush_mag;
        for (int i = 0; i < 50; i++) {
            icm20948_read_mag(&flush_mag);
            sleep_ms(10);  // Match 100Hz mag rate
        }
        printf("Buffer flushed, ready for calibration\n");
    }

    sleep_ms(500);  // Brief pause before starting calibration

    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(10, 30, "MAG CALIBRATION", COLOR_WHITE, COLOR_BLACK, 3);
    lcd_draw_string_scaled(10, 80, "Rotate device", COLOR_YELLOW, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, 110, "in figure-8", COLOR_YELLOW, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, 150, "KEY2: Done", COLOR_CYAN, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, 180, "LEFT: Cancel", COLOR_CYAN, COLOR_BLACK, 2);
    lcd_draw_string_scaled(10, LCD_HEIGHT - 25, "Time: 0s", COLOR_WHITE, COLOR_BLACK, 2);
    lcd_flush();

    // Track min/max values for each axis
    float mx_min = 1000.0f, mx_max = -1000.0f;
    float my_min = 1000.0f, my_max = -1000.0f;
    float mz_min = 1000.0f, mz_max = -1000.0f;

    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_update_ms = start_ms;
    int sample_count = 0;

    InputState input_state = {0};
    SensorData mag;

    // Prime the input state by reading it once
    input_read(&input_state);
    sleep_ms(100);

    while (1) {
        // Check for exit button
        input_read(&input_state);

        // Cancel with LEFT
        if (input_just_pressed_left(&input_state)) {
            printf("Calibration cancelled\n");
            return;
        }

        // Complete with KEY2
        if (input_just_pressed_key2(&input_state)) {
            printf("KEY2 pressed, sample_count=%d\n", sample_count);
            if (sample_count < 30) {
                lcd_clear(COLOR_BLACK);
                lcd_draw_string_scaled(20, 100, "NOT ENOUGH DATA", COLOR_RED, COLOR_BLACK, 2);
                lcd_draw_string_scaled(10, 140, "Need 30+ samples", COLOR_WHITE, COLOR_BLACK, 2);
                lcd_flush();
                sleep_ms(2000);
                return;
            }
            break;
        }

        // Read magnetometer (AK09916 runs at 100Hz, so read every 10ms minimum)
        if (icm20948_read_mag(&mag)) {
            float mx = icm20948_mag_to_ut(mag.x);
            float my = icm20948_mag_to_ut(mag.y);
            float mz = icm20948_mag_to_ut(mag.z);

            // Update min/max
            if (mx < mx_min) mx_min = mx;
            if (mx > mx_max) mx_max = mx;
            if (my < my_min) my_min = my;
            if (my > my_max) my_max = my;
            if (mz < mz_min) mz_min = mz;
            if (mz > mz_max) mz_max = mz;

            sample_count++;

            // Debug: Print every 20 samples
            if (sample_count % 20 == 0) {
                printf("Samples: %d, Mag: X=[%.1f,%.1f] Y=[%.1f,%.1f] Z=[%.1f,%.1f]\n",
                       sample_count, mx_min, mx_max, my_min, my_max, mz_min, mz_max);
            }
        } else {
            // Debug: magnetometer read failed (data not ready)
            static int fail_count = 0;
            if (++fail_count % 100 == 0) {
                printf("WARNING: Magnetometer data not ready %d times\n", fail_count);
            }
        }

        // Update display every second
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_update_ms > 1000) {
            last_update_ms = now_ms;
            uint32_t elapsed_s = (now_ms - start_ms) / 1000;

            char buf[32];
            snprintf(buf, sizeof(buf), "%ds  %d samp", elapsed_s, sample_count);
            lcd_fill_rect(0, LCD_HEIGHT - 30, LCD_WIDTH, 30, COLOR_BLACK);

            // Show green if we have enough samples, white otherwise
            uint16_t count_color = (sample_count >= 30) ? COLOR_GREEN : COLOR_WHITE;
            lcd_draw_string_scaled(10, LCD_HEIGHT - 25, buf, count_color, COLOR_BLACK, 2);
            lcd_flush();
        }

        // Sleep 10ms to match magnetometer 100Hz update rate
        sleep_ms(10);
    }

    // Calculate hard-iron offsets (midpoint of min/max)
    MagCalibration cal;
    cal.magic = MAG_CAL_MAGIC;  // Set magic first (struct field order)
    cal.offset_x = (mx_min + mx_max) / 2.0f;
    cal.offset_y = (my_min + my_max) / 2.0f;
    cal.offset_z = (mz_min + mz_max) / 2.0f;

    printf("Calibration complete:\n");
    printf("  offset_x = %.2f (range: %.2f to %.2f)\n", cal.offset_x, mx_min, mx_max);
    printf("  offset_y = %.2f (range: %.2f to %.2f)\n", cal.offset_y, my_min, my_max);
    printf("  offset_z = %.2f (range: %.2f to %.2f)\n", cal.offset_z, mz_min, mz_max);
    printf("  samples = %d\n", sample_count);

    // Save to flash
    mag_calibration_save(&cal);

    // Display results
    lcd_clear(COLOR_BLACK);
    lcd_draw_string_scaled(40, 60, "CALIBRATION", COLOR_GREEN, COLOR_BLACK, 2);
    lcd_draw_string_scaled(60, 90, "COMPLETE", COLOR_GREEN, COLOR_BLACK, 2);

    char buf[64];
    snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f", cal.offset_x, cal.offset_y, cal.offset_z);
    lcd_draw_string(10, 140, buf, COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(10, 160, "Offsets saved to flash", COLOR_CYAN, COLOR_BLACK);
    lcd_flush();

    sleep_ms(3000);
}
