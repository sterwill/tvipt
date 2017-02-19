#include "tcp.h"
#include "wifi.h"
#include "term.h"
#include "util.h"

#define TCP_COPY_LIMIT 512
#define BREAK_CHAR '\0'

static WiFiClient _client;

void tcp_loop_cb() {
    if (_client.connected()) {
        if (stream_copy_breakable(term_serial, _client, TCP_COPY_LIMIT, BREAK_CHAR)) {
            // User wants to stop connection
            _client.stop();
            return;
        }
        stream_copy(_client, term_serial, TCP_COPY_LIMIT);
    } else {
        term_writeln("");
        term_writeln("connection closed");
        wifi_set_loop_callback(NULL);
    }
}

bool tcp_connect(const char *host, uint16_t port) {
    if (!_client.connect(host, port)) {
        return false;
    }

    term_write("connected to ");
    term_write(host);
    term_write(":");
    term_println(port, DEC);

    wifi_set_loop_callback(tcp_loop_cb);
    return true;
}
