#include "ssh.h"
#include "term.h"
#include "wifi.h"
#include "util.h"

#define TCP_COPY_LIMIT 512 
#define PORT 22

void ssh_loop_cb(WiFiClient & client) {
  if (client.connected()) {
    stream_copy(term_serial, client, TCP_COPY_LIMIT);
    stream_copy(client, term_serial, TCP_COPY_LIMIT);
  } else {
    term_println("");
    term_println("connection closed");
    wifi_set_loop_callback(NULL);
  }
}

bool ssh_connect(const char * host) {
  WiFiClient & client = wifi_get_client();
  if (!client.connect(host, PORT)) {
    return false;
  }
 
  term_print("connected to ");
  term_print(host);
  term_print(":");
  term_println(PORT, DEC);

  wifi_set_loop_callback(ssh_loop_cb);
  return true;
}
