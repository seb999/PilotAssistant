/*
 * Camera Stream to LCD
 * Captures camera frames and displays them on the 240x240 ST7789 LCD
 * Uses Video4Linux2 (V4L2) for camera capture
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include "st7789_rpi.h"

// Camera configuration
#define CAMERA_DEVICE "/dev/video0"
#define CAPTURE_WIDTH  240
#define CAPTURE_HEIGHT 240
#define BUFFER_COUNT   4

// Buffer for camera frames
struct buffer {
    void *start;
    size_t length;
};

// Global variables
static int camera_fd = -1;
static struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;
static volatile bool running = true;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

// Convert YUV to RGB565
static uint16_t yuv_to_rgb565(uint8_t y, uint8_t u, uint8_t v) {
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

// Initialize camera
static int camera_init(void) {
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;

    // Open camera device
    camera_fd = open(CAMERA_DEVICE, O_RDWR);
    if (camera_fd < 0) {
        perror("Failed to open camera device");
        return -1;
    }

    // Query capabilities
    if (ioctl(camera_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("Failed to query camera capabilities");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device is not a video capture device\n");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        return -1;
    }

    // Set format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = CAPTURE_WIDTH;
    fmt.fmt.pix.height = CAPTURE_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(camera_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Failed to set camera format");
        return -1;
    }

    // Request buffers
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(camera_fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Failed to request buffers");
        return -1;
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\n");
        return -1;
    }

    // Allocate buffer structures
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }

    // Map buffers
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl(camera_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("Failed to query buffer");
            return -1;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        camera_fd, buf.m.offset);

        if (buffers[n_buffers].start == MAP_FAILED) {
            perror("Failed to mmap buffer");
            return -1;
        }
    }

    return 0;
}

// Start camera streaming
static int camera_start(void) {
    unsigned int i;
    enum v4l2_buf_type type;

    // Queue all buffers
    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(camera_fd, VIDIOC_QBUF, &buf) < 0) {
            perror("Failed to queue buffer");
            return -1;
        }
    }

    // Start streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Failed to start streaming");
        return -1;
    }

    return 0;
}

// Capture and display frame
static int camera_capture_frame(void) {
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue buffer
    if (ioctl(camera_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            return 0;
        }
        perror("Failed to dequeue buffer");
        return -1;
    }

    if (buf.index >= n_buffers) {
        fprintf(stderr, "Invalid buffer index\n");
        return -1;
    }

    // Process frame (YUYV to RGB565 and display)
    uint8_t *yuyv = (uint8_t *)buffers[buf.index].start;

    // Display frame on LCD (no rotation, direct display)
    // YUYV format: Y0 U0 Y1 V0 (4 bytes = 2 pixels sharing U and V)
    for (int y = 0; y < CAPTURE_HEIGHT; y++) {
        for (int x = 0; x < CAPTURE_WIDTH; x += 2) {
            // Process pairs of pixels (YUYV = 4 bytes)
            int idx = (y * CAPTURE_WIDTH + x) * 2;

            uint8_t y0 = yuyv[idx];
            uint8_t u  = yuyv[idx + 1];
            uint8_t y1 = yuyv[idx + 2];
            uint8_t v  = yuyv[idx + 3];

            // Convert both pixels
            uint16_t rgb565_0 = yuv_to_rgb565(y0, u, v);
            uint16_t rgb565_1 = yuv_to_rgb565(y1, u, v);

            // Draw both pixels
            lcd_draw_pixel(x, y, rgb565_0);
            lcd_draw_pixel(x + 1, y, rgb565_1);
        }
    }

    // Requeue buffer
    if (ioctl(camera_fd, VIDIOC_QBUF, &buf) < 0) {
        perror("Failed to requeue buffer");
        return -1;
    }

    return 0;
}

// Stop camera
static void camera_stop(void) {
    enum v4l2_buf_type type;

    if (camera_fd < 0) return;

    // Stop streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(camera_fd, VIDIOC_STREAMOFF, &type);

    // Unmap buffers
    for (unsigned int i = 0; i < n_buffers; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }

    // Free buffer structures
    free(buffers);
    buffers = NULL;
    n_buffers = 0;

    // Close camera
    close(camera_fd);
    camera_fd = -1;
}

int main(void) {
    int frame_count = 0;
    struct timeval start_time, current_time;
    float fps = 0.0;

    printf("=== Camera Stream to LCD ===\n");
    printf("Camera: %s\n", CAMERA_DEVICE);
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

    // Initialize camera
    printf("Initializing camera...\n");
    if (camera_init() < 0) {
        fprintf(stderr, "Failed to initialize camera\n");
        lcd_cleanup();
        return 1;
    }
    printf("✓ Camera initialized\n");

    // Start streaming
    printf("Starting camera stream...\n");
    if (camera_start() < 0) {
        fprintf(stderr, "Failed to start camera\n");
        camera_stop();
        lcd_cleanup();
        return 1;
    }
    printf("✓ Camera streaming started\n\n");

    gettimeofday(&start_time, NULL);

    // Main loop
    while (running) {
        if (camera_capture_frame() < 0) {
            break;
        }

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
    camera_stop();
    lcd_clear(COLOR_BLACK);
    lcd_cleanup();

    printf("Camera stopped and LCD cleaned up\n");

    return 0;
}
