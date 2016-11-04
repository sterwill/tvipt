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
void wifi_get_info(struct wifi_info * info);
void wifi_scan();
const char * wifi_get_status_description(int status);
const char * wifi_get_encryption_description(int type);

boolean wifi_tcp_is_connected();
boolean wifi_tcp_connect(const char * host, uint16_t port);

#endif

