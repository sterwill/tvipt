#!/bin/sh 
#
# Manpage for "arduino" command:
#
# https://github.com/arduino/Arduino/blob/master/build/shared/manpage.adoc

git clone git@github.com:sterwill/CifraChaCha20Poly1305.git ~/Arduino/libraries/CifraChaCha20Poly1305

arduino --pref "boardsmanager.additional.urls=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json" --save-prefs
arduino --install-library WiFi101:0.15.2
arduino --install-boards arduino:samd:1.6.18
arduino --install-boards adafruit:samd:1.0.22

