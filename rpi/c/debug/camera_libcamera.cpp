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
#define LCD_DISPLAY_WIDTH  240
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

// Convert RGB888 to RGB565
static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Downsample and display frame (optimized)
void display_frame_fast(const uint8_t* data, unsigned int width, unsigned int height) {
    // Use buffer to batch pixel updates
    const int x_step = width / LCD_DISPLAY_WIDTH;
    const int y_step = height / LCD_DISPLAY_HEIGHT;

    for (int lcd_y = 0; lcd_y < LCD_DISPLAY_HEIGHT; lcd_y++) {
        for (int lcd_x = 0; lcd_x < LCD_DISPLAY_WIDTH; lcd_x++) {
            int src_x = lcd_x * x_step;
            int src_y = lcd_y * y_step;
            int src_idx = (src_y * width + src_x) * 3;  // RGB888

            uint8_t r = data[src_idx];
            uint8_t g = data[src_idx + 1];
            uint8_t b = data[src_idx + 2];

            uint16_t rgb565 = rgb888_to_rgb565(r, g, b);
            lcd_draw_pixel(lcd_x, lcd_y, rgb565);
        }
    }
}

// Request completed handler
class CameraHandler : public Object {
public:
    void requestComplete(Request *request) {
        if (request->status() == Request::RequestCancelled)
            return;

        const Request::BufferMap &buffers = request->buffers();
        for (auto bufferPair : buffers) {
            FrameBuffer *buffer = bufferPair.second;
            const FrameBuffer::Plane &plane = buffer->planes()[0];

            // Map the buffer
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
};

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
    streamConfig.pixelFormat = PixelFormat::fromString("RGB888");

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

    // Create handler
    CameraHandler handler;

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

    // Connect signal
    camera->requestCompleted.connect(&handler, &CameraHandler::requestComplete);

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

    // Main loop
    while (running) {
        std::this_thread::sleep_for(1s);

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed >= 1) {
            float fps = (frame_count - last_frame_count) / (float)elapsed;
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
