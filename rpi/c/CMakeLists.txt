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

# Main application - Pilot Assistant
add_executable(pilot_assistant
    src/main.c
    src/st7789_rpi.c
    src/mpu6050.c
    src/gps.c
)

# Pico Command Receiver - receives commands from Pico2 over USB serial
add_executable(pico_receiver
    src/pico_receiver.c
    src/serial_comm.c
    src/pico_commands.c
    src/st7789_rpi.c
)

# Link libraries
target_link_libraries(pilot_assistant gpiod m)
target_link_libraries(pico_receiver gpiod m)

# Installation
install(TARGETS pilot_assistant pico_receiver
    RUNTIME DESTINATION bin
)
