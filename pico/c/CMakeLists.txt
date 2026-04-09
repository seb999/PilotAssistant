cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_telemetry_receiver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# NOTE: Old pico_telemetry_receiver removed - we only use menu_system now

# Include directories
include_directories(src drivers)

# Input test executable
add_executable(input_test
    src/main_input_test.c
    src/input_handler.c
    drivers/st7789_lcd.c
)

target_link_libraries(input_test
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_adc
    hardware_dma
)

pico_enable_stdio_usb(input_test 1)
pico_enable_stdio_uart(input_test 0)
pico_add_extra_outputs(input_test)

# Menu system executable
add_executable(menu_system
    src/main_menu.c
    src/input_handler.c
    src/menu.c
    src/telemetry_parser.c
    drivers/st7789_lcd.c
    drivers/icm20948_sensor.c
    drivers/madgwick_filter.c
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

# Command sender executable (sends button/joystick commands to Raspberry Pi)
add_executable(command_sender
    src/main_command_sender.c
    src/input_handler.c
    drivers/st7789_lcd.c
)

target_link_libraries(command_sender
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_adc
    hardware_dma
)

pico_enable_stdio_usb(command_sender 1)
pico_enable_stdio_uart(command_sender 0)
pico_add_extra_outputs(command_sender)
