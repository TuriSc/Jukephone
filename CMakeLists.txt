cmake_minimum_required(VERSION 3.13)
 
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
 
project(Jukephone C CXX ASM)
 
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
 
pico_sdk_init()

add_subdirectory(lib/RP2040-Keypad-Matrix keypad_matrix)
add_subdirectory(lib/RP2040-Battery-Check battery_check)
add_subdirectory(lib/RP2040-DFPlayer dfplayer)
add_subdirectory(lib/RP2040-Button button)
add_subdirectory(lib/RP2040-PWM-Tone pwm_tone)

add_executable(${PROJECT_NAME}
        main.c
        )

target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        keypad_matrix
        battery_check
        dfplayer
        button
        pwm_tone
        )

pico_add_extra_outputs(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

