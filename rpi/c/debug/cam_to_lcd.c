/*
 * Camera Stream to LCD using rpicam-vid (YUV420)
 * Captures camera frames and displays them on a 320x240 ST7789 LCD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "st7789_rpi.h" // adjust include path if needed

// ==================== Camera / Frame Configuration ====================

#define CAPTURE_WIDTH 320
#define CAPTURE_HEIGHT 240
// YUV420 planar: Y (W*H) + U (W*H/4) + V (W*H/4) = 1.5 * W * H bytes
#define FRAME_SIZE_YUV (CAPTURE_WIDTH * CAPTURE_HEIGHT * 3 / 2)

// ==================== Global State ====================

static volatile bool running = true;
static FILE *camera_pipe = NULL;

// ==================== Signal Handler ====================

void handle_sigint(int sig)
{
    (void)sig;
    running = false;
}

// ==================== YUV -> RGB565 Conversion ====================

static inline uint16_t yuv_to_rgb565(uint8_t y, uint8_t u, uint8_t v)
{
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;

    return (uint16_t)(((r & 0xF8) << 8) |
                      ((g & 0xFC) << 3) |
                      ((b >> 3) & 0x1F));
}

// ==================== LCD Helper ====================

// Generic helper that draws a full RGB565 frame on the LCD using lcd_draw_pixel.
// If your library later exposes a "bulk write" function, you can replace this
// with a much faster implementation.
static void lcd_draw_frame_rgb565(const uint16_t *fb, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint16_t color = fb[y * width + x];
            lcd_draw_pixel(x, y, color);
        }
    }
}

// ==================== Camera Control ====================

static FILE *camera_start(void)
{
    char cmd[512];

    // rpicam-vid:
    //   --codec yuv420  → raw YUV420 planar frames
    //   --width/height  → capture size
    //   --timeout 0     → run indefinitely
    //   -o -            → write to stdout
    // redirect stderr to /dev/null so our pipe only gets frame data
    snprintf(cmd, sizeof(cmd),
             "rpicam-vid --nopreview "
             "--codec yuv420 "
             "--width %d --height %d "
             "--framerate 30 "
             "--timeout 0 "
             "-o - 2>/dev/null",
             CAPTURE_WIDTH, CAPTURE_HEIGHT);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        perror("Failed to start rpicam-vid");
        return NULL;
    }

    return fp;
}

// ==================== Frame Processing ====================

static void process_frame(const uint8_t *frame_data, uint16_t *rgb_frame)
{
    // YUV420 planar:
    // Y plane: W * H bytes
    // U plane: (W/2) * (H/2) bytes
    // V plane: (W/2) * (H/2) bytes
    const uint8_t *y_plane = frame_data;
    const uint8_t *u_plane = y_plane + CAPTURE_WIDTH * CAPTURE_HEIGHT;
    const uint8_t *v_plane = u_plane + (CAPTURE_WIDTH * CAPTURE_HEIGHT) / 4;

    for (int y = 0; y < CAPTURE_HEIGHT; y++)
    {
        for (int x = 0; x < CAPTURE_WIDTH; x++)
        {
            int y_index = y * CAPTURE_WIDTH + x;

            int uv_x = x / 2;
            int uv_y = y / 2;
            int uv_index = uv_y * (CAPTURE_WIDTH / 2) + uv_x;

            uint8_t Y = y_plane[y_index];
            uint8_t U = u_plane[uv_index];
            uint8_t V = v_plane[uv_index];

            rgb_frame[y_index] = yuv_to_rgb565(Y, U, V);
        }
    }

    // Draw full frame to LCD
    lcd_draw_frame_rgb565(rgb_frame, CAPTURE_WIDTH, CAPTURE_HEIGHT);
}

// ==================== Main ====================

int main(void)
{
    uint8_t *frame_buffer = NULL;
    uint16_t *rgb_frame = NULL;

    int frames_this_window = 0;
    int total_frames = 0;
    float last_fps = 0.0f;

    struct timeval t_start, t_now;

    printf("=== Camera Stream to LCD (rpicam-vid, 320x240) ===\n");
    printf("Press Ctrl+C to exit\n\n");

    signal(SIGINT, handle_sigint);

    // ---------- LCD init ----------
    printf("Initializing LCD...\n");
    if (lcd_init() < 0)
    {
        fprintf(stderr, "Failed to initialize LCD\n");
        return 1;
    }
    printf("✓ LCD initialized\n");

    lcd_clear(COLOR_BLACK);
    lcd_draw_string(10, 100, "Starting camera...", COLOR_WHITE, COLOR_BLACK);

    // ---------- Buffers ----------
    frame_buffer = (uint8_t *)malloc(FRAME_SIZE_YUV);
    if (!frame_buffer)
    {
        fprintf(stderr, "Failed to allocate YUV frame buffer (%d bytes)\n", FRAME_SIZE_YUV);
        lcd_cleanup();
        return 1;
    }

    rgb_frame = (uint16_t *)malloc(CAPTURE_WIDTH * CAPTURE_HEIGHT * sizeof(uint16_t));
    if (!rgb_frame)
    {
        fprintf(stderr, "Failed to allocate RGB frame buffer\n");
        free(frame_buffer);
        lcd_cleanup();
        return 1;
    }

    // ---------- Start camera ----------
    printf("Starting camera stream...\n");
    camera_pipe = camera_start();
    if (!camera_pipe)
    {
        fprintf(stderr, "Failed to start camera (rpicam-vid)\n");
        free(frame_buffer);
        free(rgb_frame);
        lcd_cleanup();
        return 1;
    }
    printf("✓ Camera streaming started\n\n");

    gettimeofday(&t_start, NULL);

    // ---------- Main loop ----------
    while (running)
    {
        size_t bytes_read = fread(frame_buffer, 1, FRAME_SIZE_YUV, camera_pipe);

        if (bytes_read == 0)
        {
            // Probably rpicam-vid exited or failed.
            fprintf(stderr,
                    "\nNo data from camera. "
                    "Check that rpicam-vid supports '--codec yuv420' and that the camera is connected.\n");
            break;
        }

        if (bytes_read != FRAME_SIZE_YUV)
        {
            if (feof(camera_pipe))
            {
                fprintf(stderr, "Camera stream ended (EOF)\n");
                break;
            }
            if (ferror(camera_pipe))
            {
                perror("Error reading camera stream");
                break;
            }

            // Partial frame (should be rare); skip it.
            fprintf(stderr, "Warning: partial frame (%zu / %d bytes), skipping\n",
                    bytes_read, FRAME_SIZE_YUV);
            continue;
        }

        // Convert and display this frame
        process_frame(frame_buffer, rgb_frame);

        frames_this_window++;
        total_frames++;

        // FPS counter (1s window)
        gettimeofday(&t_now, NULL);
        float elapsed = (float)(t_now.tv_sec - t_start.tv_sec) +
                        (float)(t_now.tv_usec - t_start.tv_usec) / 1000000.0f;

        if (elapsed >= 1.0f)
        {
            last_fps = frames_this_window / elapsed;
            printf("FPS: %.1f   Total frames: %d\r", last_fps, total_frames);
            fflush(stdout);

            frames_this_window = 0;
            t_start = t_now;
        }
    }

    printf("\n\n=== Session Summary ===\n");
    printf("Total frames captured: %d\n", total_frames);
    printf("Last measured FPS: %.1f\n", last_fps);

    // ---------- Cleanup ----------
    if (camera_pipe)
        pclose(camera_pipe);

    free(frame_buffer);
    free(rgb_frame);

    lcd_clear(COLOR_BLACK);
    lcd_draw_string(10, 100, "Camera stopped", COLOR_WHITE, COLOR_BLACK);
    lcd_cleanup();

    printf("Camera stopped and LCD cleaned up\n");
    return 0;
}
