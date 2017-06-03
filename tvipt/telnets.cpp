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

// Read up to max bytes from stream and store in buf
size_t read(Stream &stream, byte *buf, size_t max, const char * stream_name) {
    size_t count = 0;
    while (count < max) {
        int c = stream.read();
        if (c < 0) {
            // No more bytes to read now
            break;
        }
        *buf++ = (byte) c;
        count++;
    }
    dbg_serial.write("telnets: read ");
    dbg_serial.print(count, DEC);
    dbg_serial.write(" from ");
    dbg_serial.println(stream_name);
    return count;
}

void telnets_loop_cb() {
    if (_client.connected()) {
        size_t len;

        if (term_serial.available()) {
            len = read(term_serial, _buf, BUFSIZE, "term");
            if (len <= 0) {
                _client.stop();
                return;
            }
            busybox_handle_net_output(_buf, len);
        }

        if (_client.available()) {
            len = read(_client, _buf, BUFSIZE, "net");
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
