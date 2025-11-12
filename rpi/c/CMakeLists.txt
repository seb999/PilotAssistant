cmake_minimum_required(VERSION 3.13)

# Project name
project(rpi_pilot_assistant C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find required packages (optional, for future use)
# find_package(Threads REQUIRED)

# Debug programs
add_executable(debug_adxl345
    debug/adxl345.c
)

add_executable(debug_gps
    debug/gps.c
)

add_executable(debug_lcd
    debug/lcd.c
    src/st7789_rpi.c
)

add_executable(debug_camera
    debug/camera.c
    src/st7789_rpi.c
)

# Main application - HUD Menu Display
add_executable(pilot_assistant_hud
    src/menu_hud.c
    src/st7789_rpi.c
)

# Main application - Pilot Assistant
add_executable(pilot_assistant
    src/main.c
    src/st7789_rpi.c
)

# Pico Command Receiver - receives commands from Pico2 over USB serial
add_executable(pico_receiver
    src/pico_receiver.c
    src/serial_comm.c
    src/pico_commands.c
    src/st7789_rpi.c
)

# Link libraries
target_link_libraries(debug_lcd gpiod m)
target_link_libraries(debug_camera gpiod m)
target_link_libraries(pilot_assistant_hud gpiod m)
target_link_libraries(pilot_assistant gpiod m)
target_link_libraries(pico_receiver gpiod m)
# target_link_libraries(debug_adxl345 pthread)
# target_link_libraries(debug_gps pthread)

# Installation
install(TARGETS debug_adxl345 debug_gps debug_lcd debug_camera pilot_assistant_hud pilot_assistant pico_receiver
    RUNTIME DESTINATION bin
)
