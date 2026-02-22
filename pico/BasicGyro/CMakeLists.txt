cmake_minimum_required(VERSION 3.13)

# Set board to Pico 2 BEFORE including SDK
set(PICO_BOARD pico2)

# Include Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(BasicGyro C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(BasicGyro
    main.c
    icm20948_sensor.c
    st7789_lcd.c
    madgwick.c
)

# Pull in common dependencies
target_link_libraries(BasicGyro
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_i2c
    hardware_pwm
    hardware_timer
    hardware_dma
    m  # Math library for cos, sin, etc.
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(BasicGyro 1)
pico_enable_stdio_uart(BasicGyro 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(BasicGyro)

# LCD Test executable
add_executable(test_lcd
    test_lcd.c
    st7789_lcd.c
)

target_link_libraries(test_lcd
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_dma
)

pico_enable_stdio_usb(test_lcd 1)
pico_enable_stdio_uart(test_lcd 0)
pico_add_extra_outputs(test_lcd)
