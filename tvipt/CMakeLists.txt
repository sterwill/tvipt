# This file exists to make CLion happy while editing source code.  Listing
# sources in SOURCE_FILES lets us have completion, etc.
#
# It doesn't actually compile anything correctly (use build.sh for that).

cmake_minimum_required(VERSION 3.6)
project(tvipt)

set(CMAKE_CXX_STANDARD 11)

include_directories(
    ~/.arduino15/packages/adafruit/hardware/samd/1.0.13/cores/arduino
    ~/Arduino/libraries/WiFi101/src
)

file(GLOB TVIPT_SOURCES *.ino *.cpp *.h *.c)
set(SOURCE_FILES ${TVIPT_SOURCES})

add_executable(tvipt ${SOURCE_FILES})