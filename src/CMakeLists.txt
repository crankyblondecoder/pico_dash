cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_dash C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Add extra source files here.
add_executable(pico_dash
	pico_dash.c
	pico_dash_gpio.c
	pico_dash_latch.c
	pico_dash_spi_latch.c
	X27_stepper_test.c)

# Set to 1 to enable.
pico_enable_stdio_usb(pico_dash 1)
pico_enable_stdio_uart(pico_dash 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_dash)

target_link_libraries(pico_dash pico_stdlib hardware_spi hardware_sync pico_time)
