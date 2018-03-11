#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>

// The wifi network joined automatically at boot.

#define DEFAULT_WIFI_SSID           NULL
#define DEFAULT_WIFI_PASSWORD       NULL
#define DEFAULT_WIFI_JOIN_TIMEOUT   5000

// The host to connect to using the tvipt protocol (serial over ChaCha20 + Poly1305)
// automatically at boot.

#define DEFAULT_HOST                NULL
#define DEFAULT_PORT                3333

// The Chacha20 secret key for tvipt protocol (change this to a random 32-byte
// sequence that matches the server's key).

#define SECRET_KEY_SIZE             32
#define SECRET_KEY_BYTES            {1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2}

extern const uint8_t SECRET_KEY[SECRET_KEY_SIZE];

#endif
