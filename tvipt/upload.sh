#!/bin/sh -e 
#
# Manpage for "arduino" command:
#
# https://github.com/arduino/Arduino/blob/master/build/shared/manpage.adoc

ARDUINO_HOME="$(dirname $(which arduino))"
BASE="$(realpath $(dirname ${0}))"
BUILD_PATH="${BASE}/build"

DEV="/dev/ttyACM0"

if ! [ -z "${1}" ] ; then
  DEV="${1}"
  shift
fi

stty -F "${DEV}" speed 1200 hupcl 1>/dev/null
sleep 1.5
"${HOME}/.arduino15/packages/arduino/tools/bossac/1.7.0/bossac" \
  -i --port="$(basename ${DEV})" -U true -i -e -w \
  "${BUILD_PATH}/tvipt.ino.bin" -R 
