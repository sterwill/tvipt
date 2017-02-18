#!/bin/sh -e 

BASE=$(dirname $0)
DEPS_DIR="${BASE}/deps"
DL_DIR="${BASE}/deps/dl"

get_dep() {
  URL="$1"
  EXTRACT_AT="$2"

  # Hacky, but works OK for our files
  FILE="${DL_DIR}/$(basename ${URL})"

  if ! [ -f "${FILE}" ] ; then
    mkdir -p "${DL_DIR}"
    echo "${URL} -> ${FILE}"
    curl -L -s -o "${FILE}" "${URL}"
    tar -x -v -C "${EXTRACT_AT}" -f "${FILE}"
  fi
}


get_dep "https://github.com/arduino/arduino-builder/releases/download/1.3.23/arduino-builder-linux64-1.3.23.tar.bz2" "${DEPS_DIR}"

# https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
get_dep "https://adafruit.github.io/arduino-board-index/boards/adafruit-samd-1.0.13.tar.bz2" "${DEPS_DIR}/hardware"
