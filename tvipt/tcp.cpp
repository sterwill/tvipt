#include "tcp.h"
#include "wifi.h"
#include "term.h"
#include "util.h"

#define TCP_COPY_LIMIT 512 
#define BREAK_CHAR '\0'

void tcp_loop_cb(WiFiClient & client) {
  if (client.connected()) {
    if (stream_copy_breakable(term_serial, client, TCP_COPY_LIMIT, BREAK_CHAR)) {
      // User wants to stop connection
      client.stop();
      return;
    }
    stream_copy(client, term_serial, TCP_COPY_LIMIT);
  } else {
    term_println("");
    term_println("connection closed");
    wifi_set_loop_callback(NULL);
  }
}

bool tcp_connect(const char * host, uint16_t port) {
  WiFiClient & client = wifi_get_client();
  if (!client.connect(host, port)) {
    return false;
  }
 
  term_print("connected to ");
  term_print(host);
  term_print(":");
  term_println(port, DEC);

  wifi_set_loop_callback(tcp_loop_cb);
  return true;
}
