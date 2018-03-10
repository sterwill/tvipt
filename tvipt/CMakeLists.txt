# This file exists to make CLion happy while editing source code.  Listing
# sources in SOURCE_FILES lets us have completion, etc.
#
# It doesn't actually compile anything correctly (use build.sh for that).

cmake_minimum_required(VERSION 3.6)
project(tvipt)

set(CMAKE_CXX_STANDARD 11)

# Where Arduino boards get installed.
get_filename_component(ARDUINO_PKG_DIR ~/.arduino15/packages ABSOLUTE)

# Where Arduino libraries get installed.
get_filename_component(ARDUINO_LIB_DIR ~/Arduino/libraries ABSOLUTE)

# Set the cross-compilers so it can discover the correct system
# include paths automatically.
set(CMAKE_C_COMPILER ${ARDUINO_PKG_DIR}/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${ARDUINO_PKG_DIR}/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1/bin/arm-none-eabi-gcc)

# Add the SAMD core, variant, and all libraries
include_directories(
        ${ARDUINO_PKG_DIR}/adafruit/hardware/samd/1.0.22/cores/arduino
        ${ARDUINO_PKG_DIR}/adafruit/hardware/samd/1.0.22/variants/arduino_zero
        ${ARDUINO_PKG_DIR}/arduino/tools/CMSIS/4.0.0-atmel
        ${ARDUINO_LIB_DIR}/WiFi101/src
        ${ARDUINO_LIB_DIR}/CifraChaCha20Poly1305/src
)

# Consider all source-ish files to be source code
file(GLOB TVIPT_SOURCES *.ino *.cpp *.h *.c)
set(SOURCE_FILES ${TVIPT_SOURCES})

# Pretend we'll build an executable
add_executable(tvipt ${SOURCE_FILES})