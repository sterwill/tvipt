#include <chacha20poly1305.h>

#include "wifi.h"
#include "term.h"
#include "util.h"
#include "config.h"

#define MSG_LEN_SIZE                ((uint8_t) 1)
#define MSG_NONCE_SIZE              ((uint8_t) 12)
#define MSG_TAG_SIZE                ((uint8_t) 16)

#define MSG_NONCE_OFFSET            0
#define MSG_TAG_OFFSET              (MSG_NONCE_OFFSET + MSG_NONCE_SIZE)
#define MSG_CIPHERTEXT_OFFSET       (MSG_TAG_OFFSET + MSG_TAG_SIZE)

#define MSG_LEN_MAX                 256
#define MSG_CIPHERTEXT_LEN_MAX      (MSG_LEN_MAX - MSG_NONCE_SIZE - MSG_TAG_SIZE)

#define BREAK_CHAR          '\0'

static WiFiClient _client;

// Read available characters from the terminal, encrypt them, and send them to the server.
// Returns true if no error occurred, false if the break char was read from the terminal.
bool send_available() {
    static uint64_t counter = 0;

    if (counter == 0) {
        // Initialize the counter to micros since Unix epoch, and increment for each message.
        struct wifi_info w_info;
        wifi_get_info(&w_info);
        counter = w_info.time * 1000000;
    }

    // Read all available plaintext from src
    uint8_t plaintext[MSG_CIPHERTEXT_LEN_MAX];
    uint8_t plaintext_index;
    for (plaintext_index = 0; plaintext_index < MSG_CIPHERTEXT_LEN_MAX; plaintext_index++) {
        if (term_serial.available()) {
            char c = (char) term_serial.read();
            if (c == BREAK_CHAR) {
                return false;
            }
            plaintext[plaintext_index++] = (uint8_t) (c);
        }
    }

    // Increment the counter and copy it to the low-order bytes of the nonce
    counter++;
    uint8_t nonce[MSG_NONCE_SIZE];
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce + 4, &counter, sizeof(counter));

    // Encrypt and tag
    uint8_t ciphertext[MSG_CIPHERTEXT_LEN_MAX];
    uint8_t tag[MSG_TAG_SIZE];
    cf_chacha20poly1305_encrypt(SECRET_KEY, nonce, NULL, 0, plaintext, plaintext_index, ciphertext, tag);

    // Write it
    uint8_t msg_len = MSG_LEN_SIZE + MSG_NONCE_SIZE + MSG_TAG_SIZE + plaintext_index;
    _client.write(msg_len);
    _client.write(nonce, MSG_NONCE_SIZE);
    _client.write(tag, MSG_TAG_SIZE);
    _client.write(ciphertext, plaintext_index);
    _client.flush();

    return true;
}

// Read available bytes from the server, decrypting messages as they are complete, and
// writing the decrypted text to the terminal.
//
// Returns true if no error occurred, false if an unrecoverable error occurred and the
// connection should be closed..
bool receive_available() {
    static int msg_len = -1;
    static uint8_t msg_buf[MSG_LEN_MAX];
    static uint8_t *msg_idx;

    while (_client.available()) {
        int b = _client.read();
        if (b == -1) {
            // Connection possibly closed
            return false;
        }

        // If the receive message length is -1, we're reading a new message and the first
        // byte is the length (exclusive of the length byte).
        if (msg_len == -1) {
            msg_len = (uint8_t) b;
        }

        // Read the byte into the buffer
        *msg_idx++ = (uint8_t) b;

        // Check if we have read all the message bytes into the buffer
        if (msg_idx - msg_buf == msg_len) {
            // Decode the message in the buffer
            const uint8_t *nonce = msg_buf + MSG_NONCE_OFFSET;
            const uint8_t *tag = msg_buf + MSG_TAG_OFFSET;
            const uint8_t *ciphertext = msg_buf + MSG_CIPHERTEXT_OFFSET;
            const uint8_t ciphertext_len = (uint8_t) msg_len - MSG_NONCE_SIZE - MSG_TAG_SIZE;
            uint8_t plaintext[ciphertext_len];

            // Decrypt
            if (cf_chacha20poly1305_decrypt(SECRET_KEY, nonce, NULL, 0, ciphertext, ciphertext_len,
                                            tag, plaintext) != 0) {
                term_write("error decrypting msg_len=");
                term_print(msg_len, DEC);
                term_write(" ciphertext_len=");
                term_print(ciphertext_len, DEC);
                return false;
            }

            // Write to the terminal
            term_write(plaintext, ciphertext_len);

            // Reset for the next message
            msg_len = -1;
            msg_idx = msg_buf;
        }
    }

    return true;
}

void tvipt_proto_cb() {
    if (_client.connected()) {
        if (!send_available() || !receive_available()) {
            // User wants to stop connection or there was an unrecoverable error
            _client.stop();
            return;
        }
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

    wifi_set_loop_callback(tvipt_proto_cb);
    return true;
}
