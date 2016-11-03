#include <WiFi101.h>

#include "wifi.h"
#include "term.h"
#include "util.h"

static struct wifi_info _info;

void wifi_init() {
  WiFi.setPins(8, 7, 4, 2);
  
  _info.status = WL_IDLE_STATUS;
  _info.ssid[0] = '\0';
  _info.pass[0] = '\0';
}

void on_wifi_idle(int old_wifi_status) {
  // WiFi.begin(ssid, pass);
}

void on_wifi_connected(int old_wifi_status) {
  if (old_wifi_status != WL_CONNECTED) {
    // Just connected, start SSH
  } else {
    // Pass keys to SSH
    //byte key_buf[16];
    //uint16_t key_bytes_read = read(term_serial, key_buf, sizeof(key_buf));
    //term_serial.write(key_buf, key_bytes_read);
  }
}

void wifi_loop() {
  int new_status = WiFi.status();
  switch (new_status) {
    case WL_IDLE_STATUS:
      on_wifi_idle(new_status);
      break;
    case WL_CONNECTED:
      on_wifi_connected(new_status);
      break;
  }
  _info.status = new_status;
  _info.address = WiFi.localIP();
  _info.netmask = WiFi.subnetMask();
  _info.gateway = WiFi.gatewayIP();
}

void wifi_connect(const char * ssid, const char * pass) {
  _info.status = WL_IDLE_STATUS;
  scopy(_info.ssid, ssid, sizeof(_info.ssid));
  scopy(_info.pass, pass, sizeof(_info.pass));
  _info.address[0] = '\0';
  _info.netmask[0] = '\0';
  _info.gateway[0] = '\0';
  WiFi.begin(_info.ssid, _info.pass);
}

void wifi_get_info(struct wifi_info * info) {
  info->status = _info.status;
  scopy(info->ssid, _info.ssid, sizeof(info->ssid));
  scopy(info->pass, _info.pass, sizeof(info->pass));
  info->address = _info.address;
  info->netmask = _info.netmask;
  info->gateway = _info.gateway;
}

void wifi_scan() {
  int count = WiFi.scanNetworks();
  if (count == -1) {
    term_println("scan error");
    return;
  }

  for (int i = 0; i < count; i++) {
    term_print("[");
    term_print(WiFi.SSID(i));
    term_print("] ");
    term_print(WiFi.RSSI(i));
    term_print(" dBm, ");
    term_println(wifi_get_encryption_description(WiFi.encryptionType(i)));
    Serial.flush();
  }
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



