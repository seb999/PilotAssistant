cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_telemetry_receiver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# NOTE: Old pico_telemetry_receiver removed - we only use menu_system now

# Input test executable
add_executable(input_test
    main_input_test.c
    input_handler.c
    st7789_lcd.c
)

target_link_libraries(input_test
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_adc
)

pico_enable_stdio_usb(input_test 1)
pico_enable_stdio_uart(input_test 0)
pico_add_extra_outputs(input_test)

# Menu system executable
add_executable(menu_system
    main_menu.c
    input_handler.c
    st7789_lcd.c
    menu.c
    telemetry_parser.c
)

target_link_libraries(menu_system
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_adc
    m  # Math library for cos, sin, etc.
)

pico_enable_stdio_usb(menu_system 1)
pico_enable_stdio_uart(menu_system 0)
pico_add_extra_outputs(menu_system)
