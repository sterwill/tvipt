#include "telnets.h"
#include "wifi.h"
#include "term.h"
#include "util.h"
#include "busybox.h"

#define WIDTH 80
#define HEIGHT 24
#define TERM "tvipt"

#define BUFSIZE 128
#define BREAK_CHAR '\0'

static WiFiSSLClient _client;
static byte _buf[BUFSIZE];

size_t read_buf(Stream &stream, byte *buf, size_t max) {
    size_t count = 0;
    while (count < max) {
        int c = stream.read();
        if (c < 0) {
            // No more bytes to read now
            break;
        }
        *buf++ = c;
        count++;
    }
    return count;
}

void telnets_loop_cb() {
    if (_client.connected()) {
        size_t len;

        if (term_serial.available()) {
            len = read_buf(term_serial, _buf, BUFSIZE);
            if (len <= 0) {
                _client.stop();
                return;
            }
            busybox_handle_net_output(_buf, len);
        }

        if (_client.available()) {
            len = read_buf(_client, _buf, BUFSIZE);
            if (len <= 0) {
                _client.stop();
                return;
            }
            busybox_handle_net_input(_buf, len);
        }
    } else {
        term_write("");
        term_writeln("connection closed");
        wifi_set_loop_callback(NULL);
    }
}

bool telnets_connect(const char *host, uint16_t port, const char *username) {
    if (!_client.connectSSL(host, port)) {
        term_writeln("telnets: connection failed");
        return false;
    }

    busybox_init(WIDTH, HEIGHT, TERM, &_client, username);

    wifi_set_loop_callback(telnets_loop_cb);
    return true;
}
