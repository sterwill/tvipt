# tvipt

An Internet-enabled "operating system" for the TeleVideo Personal Terminal.  Runs on an ATSAMD21 + ATWINC1500 microcontroller that can be installed inside the terminal and connects
to the terminal through its native RS-232 serial port.  tvipt lets your terminal join a Wifi network and make TCP and Telnet (over SSL) connections to the Internet.

tvipt was developed and tested with the Adafruit Feather M0 + ATWINC1500.  Arduino IDE file structure is used for portability; just open the tvipt/tvipt.ino file and make sure you have these dependencies installed:

- "Adafruit SAMD Boards" >= 1.0.13 (or other SAMD board support)
- WiFi101 library >= 0.9.1

[![TeleVideo Personal Terminal](https://raw.github.com/sterwill/tvipt/master/tvipt-small.jpg)](https://raw.github.com/sterwill/tvipt/master/tvipt.jpg)

# How it Works

The terminal connects to the controller board using just 3 wires: GND, RX, TX.  The data protoocl the terminal speaks, RS-232, uses differential signalling with positive and negative voltages way too large to feed directly into the ATSAMD21 microcontroller.  An RS-232 level shifter is used to convert the signals to a safe 3.3V, and those are connected to the RX and TX pins on the ATSAMD21.  The tvipt software does the rest.

The ATWINC1500 chip offloads Wifi, IP, TCP, and even SSL from the ATSAMD21 main processor, letting us do fun things with the limited RAM we have on the ATSAMD21.  There's not enough RAM on the ATSAMD21 to implement an SSH client properly (the SSH protocol requires clients to process 64 KB blocks of data, and there's only 64 KB RAM total!), but we can use telnet over SSL for similar security.  

# Copyright and License

Most source code in the tvipt/ directory is Copyright 2016 Shaw Terwilliger and is licened under the GNU General Public License version 2.

tvipt/busybox.cpp contains code from the BusyBox project (https://busybox.net/).  License and copyright information are available in the header of the file (GPLv2).

tvipt/jsmn.h and tvipt/jsmn.c contains code from jsmn (https://github.com/zserge/jsmn).  License and copyright information are available in the headers of those files (MIT).
