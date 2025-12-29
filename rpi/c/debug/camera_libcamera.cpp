/*
 * Camera Stream to LCD using libcamera
 * Simplified version using libcamera event dispatcher
 */

#include <iostream>
#include <memory>
#include <iomanip>
#include <signal.h>
#include <chrono>
#include <thread>
#include <sys/mman.h>
#include <libcamera/libcamera.h>

extern "C" {
#include "st7789_rpi.h"
}

using namespace libcamera;
using namespace std::chrono_literals;

// Configuration
#define CAPTURE_WIDTH  640
#define CAPTURE_HEIGHT 480
#define LCD_DISPLAY_WIDTH  320
#define LCD_DISPLAY_HEIGHT 240

// Global variables
static std::shared_ptr<Camera> camera;
static std::unique_ptr<CameraManager> cm;
static FrameBufferAllocator *allocator = nullptr;
static std::vector<std::unique_ptr<Request>> requests;
static volatile bool running = true;
static int frame_count = 0;

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

// Downsample and display frame (YUV420 format) - optimized with buffer
void display_frame_fast(const uint8_t* data, unsigned int width, unsigned int height) {
    // YUV420 planar: Y plane (full size), U plane (1/4 size), V plane (1/4 size)
    // Try swapping U and V for correct colors
    const uint8_t *y_plane = data;
    const uint8_t *v_plane = data + (width * height);  // Swapped V first
    const uint8_t *u_plane = v_plane + (width * height / 4);  // Then U

    const int x_step = width / LCD_DISPLAY_WIDTH;
    const int y_step = height / LCD_DISPLAY_HEIGHT;

    // Allocate RGB565 buffer for the entire LCD frame
    static uint16_t *rgb_buffer = nullptr;
    if (!rgb_buffer) {
        rgb_buffer = new uint16_t[LCD_DISPLAY_WIDTH * LCD_DISPLAY_HEIGHT];
    }

    // Convert YUV to RGB565 in buffer
    for (int lcd_y = 0; lcd_y < LCD_DISPLAY_HEIGHT; lcd_y++) {
        for (int lcd_x = 0; lcd_x < LCD_DISPLAY_WIDTH; lcd_x++) {
            int src_x = lcd_x * x_step;
            int src_y = lcd_y * y_step;

            // Get Y value (no stride, direct indexing)
            uint8_t y_val = y_plane[src_y * width + src_x];

            // Get U and V values (subsampled 2x2)
            int uv_x = src_x / 2;
            int uv_y = src_y / 2;
            int uv_idx = uv_y * (width / 2) + uv_x;
            uint8_t u_val = u_plane[uv_idx];
            uint8_t v_val = v_plane[uv_idx];

            rgb_buffer[lcd_y * LCD_DISPLAY_WIDTH + lcd_x] = yuv_to_rgb565(y_val, u_val, v_val);
        }
    }

    // Batch write to LCD - much faster than pixel-by-pixel
    lcd_draw_image(0, 0, LCD_DISPLAY_WIDTH, LCD_DISPLAY_HEIGHT, rgb_buffer);
}

// Request completed handler function
void requestComplete(Request *request) {
    if (request->status() == Request::RequestCancelled)
        return;

    const Request::BufferMap &buffers = request->buffers();
    for (auto bufferPair : buffers) {
        FrameBuffer *buffer = bufferPair.second;

        // YUV420 - use first plane which contains all data
        const FrameBuffer::Plane &plane = buffer->planes()[0];
        void *mem = mmap(NULL, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);

        if (mem != MAP_FAILED) {
            display_frame_fast((uint8_t*)mem, CAPTURE_WIDTH, CAPTURE_HEIGHT);
            munmap(mem, plane.length);
            frame_count++;
        }
    }

    // Requeue the request
    if (running) {
        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);
    }
}

int main() {
    std::cout << "=== Camera Stream to LCD (libcamera) ===" << std::endl;
    std::cout << "Capture: " << CAPTURE_WIDTH << "x" << CAPTURE_HEIGHT << std::endl;
    std::cout << "Display: " << LCD_DISPLAY_WIDTH << "x" << LCD_DISPLAY_HEIGHT << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;

    // Set up signal handler
    signal(SIGINT, handle_sigint);

    // Initialize LCD
    std::cout << "Initializing LCD..." << std::endl;
    if (lcd_init() < 0) {
        std::cerr << "Failed to initialize LCD" << std::endl;
        return 1;
    }
    std::cout << "✓ LCD initialized" << std::endl;

    lcd_clear(COLOR_BLACK);
    lcd_draw_string(40, 110, "Starting Camera...", COLOR_WHITE, COLOR_BLACK);

    // Initialize libcamera
    std::cout << "Initializing libcamera..." << std::endl;

    cm = std::make_unique<CameraManager>();
    int ret = cm->start();
    if (ret) {
        std::cerr << "Failed to start camera manager: " << ret << std::endl;
        lcd_cleanup();
        return 1;
    }

    if (cm->cameras().empty()) {
        std::cerr << "No cameras detected" << std::endl;
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    // Get the first camera
    std::string cameraId = cm->cameras()[0]->id();
    camera = cm->get(cameraId);
    if (!camera) {
        std::cerr << "Failed to get camera" << std::endl;
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    std::cout << "Using camera: " << cameraId << std::endl;

    // Acquire the camera
    if (camera->acquire()) {
        std::cerr << "Failed to acquire camera" << std::endl;
        camera.reset();
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    // Configure camera
    std::unique_ptr<CameraConfiguration> config =
        camera->generateConfiguration({StreamRole::Viewfinder});

    if (!config) {
        std::cerr << "Failed to generate camera configuration" << std::endl;
        camera->release();
        camera.reset();
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    StreamConfiguration &streamConfig = config->at(0);
    streamConfig.size.width = CAPTURE_WIDTH;
    streamConfig.size.height = CAPTURE_HEIGHT;
    // Use YUV420 which is better supported for streaming
    streamConfig.pixelFormat = PixelFormat::fromString("YUV420");

    config->validate();
    std::cout << "Camera configuration: " << streamConfig.toString() << std::endl;

    if (camera->configure(config.get()) < 0) {
        std::cerr << "Failed to configure camera" << std::endl;
        camera->release();
        camera.reset();
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    // Create frame buffers
    allocator = new FrameBufferAllocator(camera);
    Stream *stream = streamConfig.stream();

    ret = allocator->allocate(stream);
    if (ret < 0) {
        std::cerr << "Failed to allocate buffers: " << ret << std::endl;
        delete allocator;
        camera->release();
        camera.reset();
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    std::cout << "Allocated " << allocator->buffers(stream).size() << " buffers" << std::endl;

    // Connect signal to our callback function
    camera->requestCompleted.connect(requestComplete);

    // Create requests
    for (const std::unique_ptr<FrameBuffer> &buffer : allocator->buffers(stream)) {
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request) {
            std::cerr << "Failed to create request" << std::endl;
            delete allocator;
            camera->release();
            camera.reset();
            cm->stop();
            lcd_cleanup();
            return 1;
        }

        if (request->addBuffer(stream, buffer.get()) < 0) {
            std::cerr << "Failed to set buffer for request" << std::endl;
            delete allocator;
            camera->release();
            camera.reset();
            cm->stop();
            lcd_cleanup();
            return 1;
        }

        requests.push_back(std::move(request));
    }

    // Start camera
    std::cout << "Starting camera..." << std::endl;
    if (camera->start()) {
        std::cerr << "Failed to start camera" << std::endl;
        delete allocator;
        camera->release();
        camera.reset();
        cm->stop();
        lcd_cleanup();
        return 1;
    }

    // Queue requests
    for (std::unique_ptr<Request> &request : requests) {
        if (camera->queueRequest(request.get()) < 0) {
            std::cerr << "Failed to queue request" << std::endl;
            camera->stop();
            delete allocator;
            camera->release();
            camera.reset();
            cm->stop();
            lcd_cleanup();
            return 1;
        }
    }

    std::cout << "✓ Camera streaming started\n" << std::endl;

    // FPS counter
    auto start_time = std::chrono::steady_clock::now();
    int last_frame_count = 0;

    // Main loop - just monitor FPS, callbacks run in libcamera threads
    while (running) {
        std::this_thread::sleep_for(100ms);

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.0;

        if (elapsed >= 1.0) {
            float fps = (frame_count - last_frame_count) / elapsed;
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps << "\r" << std::flush;
            last_frame_count = frame_count;
            start_time = current_time;
        }
    }

    std::cout << "\n\n=== Session Summary ===" << std::endl;
    std::cout << "Total frames captured: " << frame_count << std::endl;

    // Cleanup
    camera->stop();
    requests.clear();
    delete allocator;
    camera->release();
    camera.reset();
    cm->stop();

    lcd_clear(COLOR_BLACK);
    lcd_cleanup();

    std::cout << "Camera stopped and LCD cleaned up" << std::endl;

    return 0;
}
