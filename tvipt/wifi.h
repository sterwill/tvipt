// WiFi features and state.

#ifndef _WIFI_H
#define _WIFI_H

#include <WiFi101.h>

struct wifi_info {
  int status;
  const char * status_description;
  char ssid[40];
  char pass[40];
  IPAddress address;
  IPAddress netmask;
  IPAddress gateway;
  uint32_t time;
  const char * firmware_version;
};

struct wifi_network {
  char ssid[40];
  int32_t rssi;
  const char * encryption_description;
};

void wifi_init();
void wifi_loop();
void wifi_connect(const char * ssid, const char * pass);
bool wifi_is_connected();
void wifi_get_info(struct wifi_info * info);
int wifi_scan(void (&scan_cb)(struct wifi_network));
WiFiClient & wifi_get_client();
void wifi_set_loop_callback(void (*loop_cb)(void));
bool wifi_has_loop_callback();

#endif

