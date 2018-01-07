#!/bin/sh 
#
# Manpage for "arduino" command:
#
# https://github.com/arduino/Arduino/blob/master/build/shared/manpage.adoc

arduino --pref "boardsmanager.additional.urls=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json" --save-prefs
arduino --install-library WiFi101:0.15.0
arduino --install-boards adafruit:samd:1.0.17
