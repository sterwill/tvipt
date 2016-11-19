// Busybox's telnet client hacked up for WiFi101.

#ifndef _BUSYBOX_H
#define _BUSYBOX_H

#include <Arduino.h>
#include <WiFi101.h>

#define ENABLE_FEATURE_TELNET_TTYPE 1
#define ENABLE_FEATURE_TELNET_AUTOLOGIN 1
#define ENABLE_FEATURE_AUTOWIDTH 1

void busybox_init(unsigned int win_width, unsigned int win_height, char * ttype, WiFiClient * client, const char * autologin);
void busybox_handle_net_output(byte * buf, int len);
void busybox_handle_net_input(byte * buf, int len);

#endif
