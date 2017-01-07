#include <WiFi101.h>

#include "wifi.h"
#include "term.h"
#include "util.h"

static char _ssid[40];
static char _pass[40];
static void (*_loop_cb)();

void wifi_init() {
  WiFi.setPins(8, 7, 4, 2);
  _ssid[0] = '\0';
  _pass[0] = '\0';
}

void wifi_loop() {
  if (_loop_cb != NULL) {
    _loop_cb();
  }
}

void wifi_connect(const char * ssid, const char * pass) {
  scopy(_ssid, ssid, sizeof(_ssid));
  scopy(_pass, pass, sizeof(_pass));
  WiFi.begin(_ssid, _pass);
}

bool wifi_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}

const char * wifi_get_status_description(int status) {
  switch (status) {
    case WL_CONNECTED:
      return "connected";
    case WL_NO_SHIELD:
      return "no shield";
    case WL_IDLE_STATUS:
      return "idle";
    case WL_NO_SSID_AVAIL:
      return "no ssid";
    case WL_SCAN_COMPLETED:
      return "scan completed";
    case WL_CONNECT_FAILED:
      return "connect failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

const char * wifi_get_encryption_description(int type) {
  switch (type) {
    case ENC_TYPE_WEP:
      return "WEP";
    case ENC_TYPE_TKIP:
      return "WPA";
    case ENC_TYPE_CCMP:
      return "WPA2";
    case ENC_TYPE_NONE:
      return "none";
    case ENC_TYPE_AUTO:
      return "auto";
    default:
      return "unknown";    
  }
}

void wifi_get_info(struct wifi_info * info) {
  info->status = WiFi.status();
  info->status_description = wifi_get_status_description(info->status);
  scopy(info->ssid, _ssid, sizeof(info->ssid));
  scopy(info->pass, _pass, sizeof(info->pass));
  info->address = WiFi.localIP();
  info->netmask = WiFi.subnetMask();
  info->gateway = WiFi.gatewayIP();
  info->time = WiFi.getTime();
  info->firmware_version = WiFi.firmwareVersion();
}

int wifi_scan(void (&scan_cb)(struct wifi_network)) {
  int count = WiFi.scanNetworks();
  if (count == -1) {
    return count;
  }

  struct wifi_network net;
  for (int i = 0; i < count; i++) {
    scopy(net.ssid, WiFi.SSID(i), sizeof(net.ssid));
    net.rssi = WiFi.RSSI(i);
    net.encryption_description = wifi_get_encryption_description(WiFi.encryptionType(i));
    scan_cb(net);
  }
}

void wifi_set_loop_callback(void (*loop_cb)()) {
  _loop_cb = loop_cb;
}

bool wifi_has_loop_callback() {
  return _loop_cb != NULL;
}

