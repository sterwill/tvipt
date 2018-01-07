#!/bin/sh -e 
#
# Manpage for "arduino" command:
#
# https://github.com/arduino/Arduino/blob/master/build/shared/manpage.adoc

ARDUINO_HOME="$(dirname $(which arduino))"
BASE="$(realpath $(dirname ${0}))"
BUILD_PATH="${BASE}/build"

mkdir -p "${BUILD_PATH}"

arduino-builder -compile \
  -hardware "${ARDUINO_HOME}/hardware" \
  -hardware "${HOME}/.arduino15/packages" \
  -tools "${ARDUINO_HOME}/tools-builder" \
  -tools "${ARDUINO_HOME}/hardware/tools/avr" \
  -tools "${HOME}/.arduino15/packages" \
  -built-in-libraries "${ARDUINO_HOME}/libraries" \
  -libraries "${HOME}/Arduino/libraries" \
  -fqbn=adafruit:samd:adafruit_feather_m0 \
  -vid-pid=0X239A_0X800B \
  -ide-version=10801 \
  -build-path "${BUILD_PATH}" \
  -warnings=none \
  -prefs=build.warn_data_percentage=75  \
  -prefs="runtime.tools.openocd.path=${HOME}/.arduino15/packages/arduino/tools/openocd/0.9.0-arduino6-static"  \
  -prefs="runtime.tools.bossac.path=${HOME}/.arduino15/packages/arduino/tools/bossac/1.7.0" \
  -prefs="runtime.tools.CMSIS.path=${HOME}/.arduino15/packages/arduino/tools/CMSIS/4.5.0" \
  -prefs="runtime.tools.arm-none-eabi-gcc.path=${HOME}/.arduino15/packages/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1" \
  $@ \
  "${BASE}/tvipt.ino" 
