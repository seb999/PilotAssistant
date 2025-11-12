/*
 * Camera Stream to LCD using libcamera
 * Captures camera frames using rpicam-vid and displays them on the 240x240 ST7789 LCD
 * Uses pipe to get raw video data from rpicam-vid
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "st7789_rpi.h"

// Camera configuration
#define CAPTURE_WIDTH  240
#define CAPTURE_HEIGHT 240
#define FRAME_SIZE_YUV (CAPTURE_WIDTH * CAPTURE_HEIGHT * 3 / 2) // YUV420 (1.5 bytes per pixel)

// Global variables
static volatile bool running = true;
static FILE *camera_pipe = NULL;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

// Convert YUV to RGB565
static inline uint16_t yuv_to_rgb565(uint8_t y, uint8_t u, uint8_t v) {
    int r, g, b;

    // YUV to RGB conversion
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    r = (298 * c + 409 * e + 128) >> 8;
    g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    b = (298 * c + 516 * d + 128) >> 8;

    // Clamp to 0-255
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    // Convert to RGB565
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Start camera streaming via rpicam-vid
static FILE* camera_start(void) {
    char cmd[512];

    // Use rpicam-vid to output raw YUV420 frames
    // --nopreview: no X11 preview window
    // --codec yuv420: raw YUV output
    // --width/height: capture resolution
    // --timeout 0: run indefinitely
    // -o -: output to stdout
    snprintf(cmd, sizeof(cmd),
        "rpicam-vid --nopreview --codec yuv420 --width %d --height %d --timeout 0 -o - 2>/dev/null",
        CAPTURE_WIDTH, CAPTURE_HEIGHT);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("Failed to start rpicam-vid");
        return NULL;
    }

    return fp;
}

// Process and display a YUV420 frame
static int process_frame(uint8_t *frame_data) {
    // YUV420 format: Y plane (full size), U plane (1/4 size), V plane (1/4 size)
    uint8_t *y_plane = frame_data;
    uint8_t *u_plane = frame_data + (CAPTURE_WIDTH * CAPTURE_HEIGHT);
    uint8_t *v_plane = u_plane + (CAPTURE_WIDTH * CAPTURE_HEIGHT / 4);

    // Display frame on LCD pixel by pixel
    for (int y = 0; y < CAPTURE_HEIGHT; y++) {
        for (int x = 0; x < CAPTURE_WIDTH; x++) {
            // Get Y value
            uint8_t y_val = y_plane[y * CAPTURE_WIDTH + x];

            // Get U and V values (subsampled 2x2)
            int uv_x = x / 2;
            int uv_y = y / 2;
            int uv_idx = uv_y * (CAPTURE_WIDTH / 2) + uv_x;
            uint8_t u_val = u_plane[uv_idx];
            uint8_t v_val = v_plane[uv_idx];

            uint16_t rgb565 = yuv_to_rgb565(y_val, u_val, v_val);
            lcd_draw_pixel(x, y, rgb565);
        }
    }

    return 0;
}

int main(void) {
    int frame_count = 0;
    struct timeval start_time, current_time;
    float fps = 0.0;
    uint8_t *frame_buffer = NULL;

    printf("=== Camera Stream to LCD (libcamera) ===\n");
    printf("Resolution: %dx%d\n", CAPTURE_WIDTH, CAPTURE_HEIGHT);
    printf("Press Ctrl+C to exit\n\n");

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize LCD
    printf("Initializing LCD...\n");
    if (lcd_init() < 0) {
        fprintf(stderr, "Failed to initialize LCD\n");
        return 1;
    }
    printf("✓ LCD initialized\n");

    lcd_clear(COLOR_BLACK);
    lcd_draw_string(40, 110, "Starting Camera...", COLOR_WHITE, COLOR_BLACK);

    // Allocate frame buffer for YUV420
    frame_buffer = (uint8_t *)malloc(FRAME_SIZE_YUV);
    if (!frame_buffer) {
        fprintf(stderr, "Failed to allocate frame buffer\n");
        lcd_cleanup();
        return 1;
    }

    // Start camera
    printf("Starting camera stream...\n");
    camera_pipe = camera_start();
    if (!camera_pipe) {
        fprintf(stderr, "Failed to start camera\n");
        free(frame_buffer);
        lcd_cleanup();
        return 1;
    }
    printf("✓ Camera streaming started\n\n");

    gettimeofday(&start_time, NULL);

    // Main loop - read frames from pipe
    while (running) {
        size_t bytes_read = fread(frame_buffer, 1, FRAME_SIZE_YUV, camera_pipe);

        if (bytes_read != FRAME_SIZE_YUV) {
            if (feof(camera_pipe)) {
                fprintf(stderr, "Camera stream ended unexpectedly\n");
                break;
            }
            if (ferror(camera_pipe)) {
                fprintf(stderr, "Error reading camera stream\n");
                break;
            }
            continue;
        }

        // Process and display the frame
        process_frame(frame_buffer);

        frame_count++;

        // Calculate FPS every second
        gettimeofday(&current_time, NULL);
        float elapsed = (current_time.tv_sec - start_time.tv_sec) +
                       (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

        if (elapsed >= 1.0) {
            fps = frame_count / elapsed;
            printf("FPS: %.1f\r", fps);
            fflush(stdout);

            frame_count = 0;
            start_time = current_time;
        }
    }

    printf("\n\n=== Session Summary ===\n");
    printf("Total frames captured: %d\n", frame_count);
    printf("Average FPS: %.1f\n", fps);

    // Cleanup
    if (camera_pipe) {
        pclose(camera_pipe);
    }
    free(frame_buffer);
    lcd_clear(COLOR_BLACK);
    lcd_cleanup();

    printf("Camera stopped and LCD cleaned up\n");

    return 0;
}
