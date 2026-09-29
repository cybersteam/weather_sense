cmake_minimum_required(VERSION 3.16)
project(weather_sense C)

set(WS_TARGET "host" CACHE STRING "Build target: host or avr")
set_property(CACHE WS_TARGET PROPERTY STRINGS host avr)

# Consumed by CMake itself on the first configure. Referencing it here keeps a
# later `cmake -DCMAKE_TOOLCHAIN_FILE=...` from warning that the project
# never read the variable.
if(CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(WS_SOURCES
    src/crc16.c
    src/format.c
    src/delay.c
    src/uart_text.c
    src/metrics.c
    src/bme280.c
    src/keypad.c
    src/config_store.c
    src/protocol.c
    src/ui.c
    src/lcd.c
    src/station.c
)

function(ws_warnings target)
    target_compile_options(${target} PRIVATE
        -std=c11
        -Wall
        -Wextra
        -Werror
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wshadow
        -Wvla
        -Wundef
        -Wpointer-arith
        -ffunction-sections
        -fdata-sections
    )
endfunction()

if(WS_TARGET STREQUAL "avr")
    if(NOT CMAKE_C_COMPILER MATCHES "avr-gcc")
        message(FATAL_ERROR
            "WS_TARGET=avr requires -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/avr-gcc.cmake")
    endif()
    if(NOT AVR_OBJCOPY)
        set(AVR_OBJCOPY avr-objcopy)
    endif()
    if(NOT AVR_SIZE)
        set(AVR_SIZE avr-size)
    endif()

    add_executable(weather_sense ${WS_SOURCES} src/main.c src/hal_avr.c)
    target_include_directories(weather_sense PRIVATE include)
    target_compile_definitions(weather_sense PRIVATE F_CPU=16000000UL)
    ws_warnings(weather_sense)
    target_compile_options(weather_sense PRIVATE -mmcu=atmega328p -Os -mrelax)
    target_link_options(weather_sense PRIVATE
        -mmcu=atmega328p
        -mrelax
        -Wl,--gc-sections
        -Wl,-Map=weather_sense.map
    )
    add_custom_command(TARGET weather_sense POST_BUILD
        COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:weather_sense>
                ${CMAKE_BINARY_DIR}/weather_sense.hex
        COMMAND ${CMAKE_COMMAND} -E env SIZE_CMD=${AVR_SIZE}
                python3 ${CMAKE_SOURCE_DIR}/scripts/size_gate.py $<TARGET_FILE:weather_sense>
        VERBATIM
    )
elseif(WS_TARGET STREQUAL "host")
    enable_testing()
    add_executable(ws_tests
        ${WS_SOURCES}
        src/hal_host.c
        tests/harness.c
        tests/fake_bme.c
        tests/test_unit.c
        tests/test_bme.c
        tests/test_ui.c
        tests/test_station.c
        tests/test_main.c
    )
    target_include_directories(ws_tests PRIVATE include tests)
    target_compile_definitions(ws_tests PRIVATE WS_HOST=1)
    ws_warnings(ws_tests)
    target_compile_options(ws_tests PRIVATE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(ws_tests PRIVATE -fsanitize=address,undefined)
    add_test(NAME ws_tests COMMAND ws_tests)
else()
    message(FATAL_ERROR "WS_TARGET must be host or avr, got '${WS_TARGET}'")
endif()
