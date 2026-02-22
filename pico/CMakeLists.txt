cmake_minimum_required(VERSION 3.13)

# Set board to Pico 2 BEFORE including SDK (RP2350 with 520KB RAM vs RP2040 264KB RAM)
set(PICO_BOARD pico2)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pilot_assistant C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Menu system executable (main application)
add_executable(menu_system
    main_menu.c
    menu.c
    telemetry_parser.c
    attitude_indicator.c
    drivers/input_handler.c
    drivers/st7789_lcd.c
    drivers/icm20948_sensor.c
    drivers/madgwick_filter.c
)

target_include_directories(menu_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
)

target_link_libraries(menu_system
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_adc
    hardware_dma
    m  # Math library for cos, sin, etc.
)

pico_enable_stdio_usb(menu_system 1)
pico_enable_stdio_uart(menu_system 0)
pico_add_extra_outputs(menu_system)
