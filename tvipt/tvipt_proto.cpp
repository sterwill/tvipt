#include <chacha20poly1305.h>
#include "wifi.h"
#include "term.h"
#include "util.h"
#include "config.h"

#define MSG_LEN_SIZE        2
#define MSG_NONCE_SIZE      12
#define MSG_MAC_SIZE        16

#define TCP_COPY_LIMIT 512
#define BREAK_CHAR '\0'

static const uint8_t _secret_key[] = SECRET_KEY;
static uint64_t _counter;
static WiFiClient _client;

// Copy text returning true if the break char is read from src, false otherwise
bool stream_copy_breakable_crypt(Stream &src, Stream &dst, uint16_t max_bytes, const char break_char) {
    uint8_t plaintext[max_bytes];

    uint16_t plaintext_index;
    for (plaintext_index = 0; plaintext_index < max_bytes; plaintext_index++) {
        if (src.available()) {
            char c = src.read();
            if (c == break_char) {
                return true;
            }
            plaintext[plaintext_index++] = (uint8_t) (c);
        }
    }

    _chacha.clear();

    // Set the secret key
    _chacha.setKey(_secret_key, sizeof(_secret_key));

    // Set the nonce
//    uint8_t nonce[MSG_NONCE_SIZE];
//    memcpy()
    uint64_t nonce = _counter++;
//    _chacha.setIV(nonce, sizeof(nonce));

    uint8_t ciphertext[max_bytes];
    _chacha.encrypt(ciphertext, plaintext, plaintext_index);

    uint8_t mac[16];
    _chacha.computeTag(mac, sizeof(mac));

    uint16_t msg_len = MSG_LEN_SIZE + MSG_NONCE_SIZE + MSG_MAC_SIZE + plaintext_index;

    // Write message length, big endian
    dst.write((uint8_t) ((msg_len >> 8) & 0xFF));
    dst.write((uint8_t) (msg_len & 0xFF));

    // Write the nonce (pad to 12 bytes)


    // Write the MAC
    for (uint8_t mac_byte : mac) {
        dst.write(mac_byte);
    }

    // Write the ciphertext
    for (uint8_t ciphertext_byte : ciphertext) {
        dst.write(ciphertext_byte);
    }

    return false;
}


void tvipt_proto_cb() {
    if (_client.connected()) {
        if (stream_copy_breakable_crypt(term_serial, _client, TCP_COPY_LIMIT, BREAK_CHAR)) {
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

/*
 * Connects to a tvipt server.  The protocol is encrypted and uses very simple framing
 * (which might render it not-very-secure; use at your own risk).  The encryption and
 * MAC scheme is ChaCha20-Poly1305 (original construction).
 *
 * https://download.libsodium.org/doc/secret-key_cryptography/original_chacha20-poly1305_construction.html
 *
 *  A serialized message looks like:
 *
 *  Byte  Example  Description
 *  ---------------------------
 *  1     0x02     message length high byte
 *  2     0x08     message length low byte
 *
 *                 The first two bytes are a big-endian unsigned 16-bit integer that
 *                 specifies the number of message bytes that follow.  In this example,
 *                 the value 0x0208 means the message will contain 520 bytes after this
 *                 length header.
 *
 *  3     0xAB     first nonce byte (12 bytes)
 *  ...   ...      ...
 *  14    0x7E     last nonce byte
 *
 *  15    0xF0     first MAC byte (16 bytes)
 *  ...   ...      ...
 *  30    0x0F     last MAC byte
 *
 *  31    0x00     first ciphertext byte
 *  ...   ...
 *  522   0xFF     last ciphertext byte
 */
bool tvipt_proto_connect(const char *host, uint16_t port) {
    if (!_client.connect(host, port)) {
        return false;
    }

    term_write("connected to ");
    term_write(host);
    term_write(":");
    term_println(port, DEC);

    // Initialize a counter to micros since Unix epoch.  Used as a nonce for Chacha20 and
    // incremented once for each message sent.
    struct wifi_info w_info;
    wifi_get_info(&w_info);
    uint64_t counter = w_info.time * 1000000;
    term_write("counter=");
    term_println(counter, DEC);

    wifi_set_loop_callback(tvipt_proto_cb);
    return true;
}
