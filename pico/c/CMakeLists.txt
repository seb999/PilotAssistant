cmake_minimum_required(VERSION 3.13)

# Set board to Pico 2W BEFORE including SDK (required for WiFi support)
set(PICO_BOARD pico2_w)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_telemetry_receiver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# NOTE: Old pico_telemetry_receiver removed - we only use menu_system now

# Include directories
include_directories(src drivers . assets)

# mbedtls config lives at project root
set(MBEDTLS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_config.h" CACHE STRING "")

# Input test executable (DISABLED - no joystick connected)
# add_executable(input_test
#     src/main_input_test.c
#     src/input_handler.c
#     drivers/st7789_lcd.c
# )
#
# target_link_libraries(input_test
#     pico_stdlib
#     hardware_gpio
#     hardware_spi
#     hardware_adc
#     hardware_dma
# )
#
# pico_enable_stdio_usb(input_test 1)
# pico_enable_stdio_uart(input_test 0)
# pico_add_extra_outputs(input_test)

# Menu system executable (Pico 2W with WiFi, standalone, dual-core AHRS)
add_executable(menu_system
    src/main_menu.c
    src/menu.c
    drivers/st7789_lcd.c
    drivers/icm20948_sensor.c
    drivers/madgwick_filter.c
    drivers/xpt2046_touch.c
    drivers/wifi_manager.c
    drivers/opensky_client.c
    drivers/bluetooth_manager.c
    drivers/ahrs_core.c
)

target_link_libraries(menu_system
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_spi
    hardware_adc
    hardware_dma
    pico_cyw43_arch_lwip_poll
    pico_lwip_mbedtls
    pico_mbedtls
    m
)

# lwipopts.h and mbedtls_config.h live at the project root
target_include_directories(menu_system PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

pico_enable_stdio_usb(menu_system 1)
pico_enable_stdio_uart(menu_system 0)
pico_add_extra_outputs(menu_system)

# ICM20948 SPI test
add_executable(icm_test
    src/main_icm_test.c
    drivers/icm20948_sensor.c
)

target_link_libraries(icm_test
    pico_stdlib
    hardware_gpio
    hardware_spi
)

pico_enable_stdio_usb(icm_test 1)
pico_enable_stdio_uart(icm_test 0)
pico_add_extra_outputs(icm_test)

# Touch screen test executable
add_executable(touch_test
    src/main_touch_test.c
    drivers/st7789_lcd.c
    drivers/xpt2046_touch.c
)

target_link_libraries(touch_test
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_dma
)

pico_enable_stdio_usb(touch_test 1)
pico_enable_stdio_uart(touch_test 0)
pico_add_extra_outputs(touch_test)

# Standalone AHRS test (no touchscreen, pure gyro testing)
add_executable(ahrs_test
    src/main_ahrs_standalone.c
    drivers/st7789_lcd.c
    drivers/icm20948_sensor.c
    drivers/madgwick_filter.c
)

target_link_libraries(ahrs_test
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_dma
    m
)

pico_enable_stdio_usb(ahrs_test 1)
pico_enable_stdio_uart(ahrs_test 0)
pico_add_extra_outputs(ahrs_test)

# Command sender executable (DISABLED - no joystick connected)
# add_executable(command_sender
#     src/main_command_sender.c
#     src/input_handler.c
#     drivers/st7789_lcd.c
# )
#
# target_link_libraries(command_sender
#     pico_stdlib
#     hardware_gpio
#     hardware_spi
#     hardware_adc
#     hardware_dma
# )
#
# pico_enable_stdio_usb(command_sender 1)
# pico_enable_stdio_uart(command_sender 0)
# pico_add_extra_outputs(command_sender)
