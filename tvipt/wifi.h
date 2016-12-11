// WiFi features and state.

#ifndef _WIFI_H
#define _WIFI_H

#include <WiFi101.h>

struct wifi_info {
  int status;
  char ssid[40];
  char pass[40];
  IPAddress address;
  IPAddress netmask;
  IPAddress gateway;
};

void wifi_init();
void wifi_loop();
void wifi_connect(const char * ssid, const char * pass);
bool wifi_is_connected();
void wifi_get_info(struct wifi_info * info);
void wifi_scan();
const char * wifi_get_status_description(int status);
const char * wifi_get_encryption_description(int type);
WiFiClient & wifi_get_client();
void wifi_set_loop_callback(void (*loop_cb)(void));
bool wifi_has_loop_callback();
const char * wifi_get_firmware_version();

#endif

