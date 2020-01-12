#include <chacha20poly1305.h>

#include "wifi.h"
#include "term.h"
#include "util.h"
#include "config.h"

#define MSG_NONCE_SIZE              ((uint8_t) 12)
#define MSG_TAG_SIZE                ((uint8_t) 16)
#define MSG_CIPHERTEXT_LEN_SIZE     ((uint8_t) 1)

#define MSG_NONCE_OFFSET            0
#define MSG_TAG_OFFSET              (MSG_NONCE_OFFSET + MSG_NONCE_SIZE)
#define MSG_CIPHERTEXT_LEN_OFFSET   (MSG_TAG_OFFSET + MSG_TAG_SIZE)
#define MSG_CIPHERTEXT_OFFSET       (MSG_CIPHERTEXT_LEN_OFFSET + MSG_CIPHERTEXT_LEN_SIZE)

#define MSG_CIPHERTEXT_LEN_MAX      255
#define BREAK_CHAR                  '\0'

// Uncomment to print encryption information to dbg_serial
//#define DEBUG_CRYPT

static WiFiClient _client;

// Read available characters from the terminal, encrypt them, and send them to the server.
// Returns true if no error occurred, false if the break char was read from the terminal.
bool send_available() {
    static uint64_t counter = 0;

    if (counter == 0) {
        // Initialize the counter to micros since Unix epoch, and increment for each message.
        struct wifi_info w_info;
        wifi_get_info(&w_info);
        if (w_info.time == 0) {
            // No network time yet; can't send data safely without a proper counter.
            return true;
        }
        counter = (uint64_t) w_info.time * 1000000;
#ifdef DEBUG_CRYPT
        hexdump(dbg_serial, "initial counter", &counter, sizeof(counter));
#endif
    }

    // Read all available plaintext from src
    uint8_t plaintext[MSG_CIPHERTEXT_LEN_MAX];
    uint8_t plaintext_size;
    for (plaintext_size = 0; plaintext_size < MSG_CIPHERTEXT_LEN_MAX && term_serial.available(); plaintext_size++) {
        char c = (char) term_serial.read();
        if (c == BREAK_CHAR) {
            term_writeln("break!");
            return false;
        }
        plaintext[plaintext_size] = (uint8_t) (c);
    }

    if (plaintext_size == 0) {
        // No chars ready
        return true;
    }

    // Increment the counter
    counter++;
    uint64_t counter_n = _htonll(counter);

    // Use the counter as the low-order bytes of the nonce
    uint8_t nonce[MSG_NONCE_SIZE];
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce + 4, &counter_n, sizeof(counter_n));

    // Encrypt and tag
    uint8_t ciphertext[MSG_CIPHERTEXT_LEN_MAX];
    uint8_t tag[MSG_TAG_SIZE];
    cf_chacha20poly1305_encrypt(SECRET_KEY, nonce, NULL, 0, plaintext, plaintext_size, ciphertext, tag);

#ifdef DEBUG_CRYPT
    hexdump(dbg_serial, "send nonce", nonce, sizeof(nonce));
    hexdump(dbg_serial, "send tag", tag, sizeof(tag));
    hexdump(dbg_serial, "send ciphertext len", &plaintext_size, sizeof(plaintext_size));
    hexdump(dbg_serial, "send ciphertext", ciphertext, plaintext_size);
#endif

    // Write it
    _client.write(nonce, MSG_NONCE_SIZE);
    _client.write(tag, MSG_TAG_SIZE);
    _client.write(plaintext_size);
    _client.write(ciphertext, plaintext_size);
    _client.flush();

#ifdef DEBUG_CRYPT
    hexdump(dbg_serial, "send done", nonce, sizeof(nonce));
#endif

    return true;
}

// Read available bytes from the server, decrypting messages as they are complete, and
// writing the decrypted text to the terminal.
//
// Returns true if no error occurred, false if an unrecoverable error occurred and the
// connection should be closed..
bool receive_available() {
    static uint8_t msg_buf[MSG_NONCE_SIZE + MSG_TAG_SIZE + MSG_CIPHERTEXT_LEN_SIZE + MSG_CIPHERTEXT_LEN_MAX];
    static uint8_t *msg_idx = msg_buf;

    while (_client.available()) {
        int b = _client.read();
        if (b == -1) {
            // Connection possibly closed
            return false;
        }

        // Read the byte into the buffer
        *msg_idx++ = (uint8_t) b;

        // Check if we have a complete message in the buffer (index points past nonce + tag + ciphertext len
        // and index is at the last byte of the ciphertext data).
        if (msg_idx >= (msg_buf + MSG_NONCE_SIZE + MSG_TAG_SIZE + MSG_CIPHERTEXT_LEN_SIZE)
            && msg_idx == msg_buf + MSG_CIPHERTEXT_OFFSET + msg_buf[MSG_CIPHERTEXT_LEN_OFFSET]) {
            // Decode the message in the buffer
            const uint8_t *nonce = msg_buf + MSG_NONCE_OFFSET;
            const uint8_t *tag = msg_buf + MSG_TAG_OFFSET;
            const uint8_t ciphertext_len = msg_buf[MSG_CIPHERTEXT_LEN_OFFSET];
            const uint8_t *ciphertext = msg_buf + MSG_CIPHERTEXT_OFFSET;
            uint8_t plaintext[ciphertext_len];

#ifdef DEBUG_CRYPT
            hexdump(dbg_serial, "recv nonce", nonce, MSG_NONCE_SIZE);
            hexdump(dbg_serial, "recv tag", tag, MSG_TAG_SIZE);
            hexdump(dbg_serial, "recv ciphertext_len", &ciphertext_len, sizeof(ciphertext_len));
            hexdump(dbg_serial, "recv ciphertext", ciphertext, ciphertext_len);
#endif

            // Decrypt
            if (cf_chacha20poly1305_decrypt(SECRET_KEY, nonce, NULL, 0, ciphertext, ciphertext_len, tag,
                                            plaintext) != 0) {
                term_write("decrypt error");
                return false;
            }

#ifdef DEBUG_CRYPT
            hexdump(dbg_serial, "recv plaintext", plaintext, ciphertext_len);
#endif
            // Write to the terminal
            term_write(plaintext, ciphertext_len);

            // Reset for the next message
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
 * MAC scheme is ChaCha20-Poly1305 (12-byte nonce and 16-byte tag).
 *
 * The nonce is initialized as the microseconds since the Unix epoch according to network
 * time).  It is incremented by 1 for each message sent during the connection.  Assuming
 * network time is reasonably accurate, and that is impossible to increment the
 * nonce more than 1 million times per second (sending that many messages per second
 * is impossible on this hardware), the nonce never gets ahead of the current time and
 * so it can never be repeated in a future session.
 *
 * https://download.libsodium.org/doc/secret-key_cryptography/original_chacha20-poly1305_construction.html
 *
 *  A serialized message looks like:
 *
 *  Byte  Example  Description
 *  ---------------------------
 *
 *  1     0xAB     first nonce byte (12 bytes total)
 *  ...   ...      ...
 *  12    0x7E     last nonce byte
 *
 *  13    0xF0     first tag byte (16 bytes total)
 *  ...   ...      ...
 *  28    0x0F     last tag byte
 *
 *  29    0x09     ciphertext length (1 byte)
 *
 *                 The ciphertext part of a message is between 1 and 255 bytes
 *                 in length (0 is invalid).
 *
 *  30    0x00     first ciphertext byte
 *  ...   ...
 *  39    0xFF     last ciphertext byte
 */
bool tvipt_proto_connect(const char *host, uint16_t port) {
    if (!_client.connect(host, port)) {
        return false;
    }

    term_write("connected to ");
    term_write(host);
    term_write(":");
    term_println(port, DEC);

    // Eat buffered input (this can be garbage after a reboot of the terminal)
    while (term_serial.available()) {
        term_serial.read();
    }

    wifi_set_loop_callback(tvipt_proto_cb);
    return true;
}
