# Bare-metal AVR toolchain. Override the prefix when avr-gcc is not on PATH:
#   AVR_PREFIX=$HOME/.local/avr cmake -DCMAKE_TOOLCHAIN_FILE=cmake/avr-gcc.cmake ...
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

if(DEFINED ENV{AVR_PREFIX} AND NOT "$ENV{AVR_PREFIX}" STREQUAL "")
    set(_ws_avr "$ENV{AVR_PREFIX}/bin/")
else()
    set(_ws_avr "")
endif()

set(CMAKE_C_COMPILER "${_ws_avr}avr-gcc")
set(CMAKE_ASM_COMPILER "${_ws_avr}avr-gcc")
set(CMAKE_AR "${_ws_avr}avr-ar")
set(CMAKE_RANLIB "${_ws_avr}avr-ranlib")
set(AVR_OBJCOPY "${_ws_avr}avr-objcopy" CACHE FILEPATH "avr-objcopy")
set(AVR_SIZE "${_ws_avr}avr-size" CACHE FILEPATH "avr-size")

# project() probes the compiler before the target flags in CMakeLists exist.
set(CMAKE_C_FLAGS_INIT "-mmcu=atmega328p")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mmcu=atmega328p")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
