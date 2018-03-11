// Utility functions.

#ifndef _UTIL_H
#define _UTIL_H

#include <Arduino.h>
#include <string.h>
#include "jsmn.h"
#include "term.h"

// Like strncpy, but makes sure dest is terminated.
inline char *scopy(char *dest, const char *src, size_t n) {
    strncpy(dest, src, n - 1);
    if (n > 0) {
        dest[n - 1] = '\0';
    }
    return dest;
}

inline uint16_t stream_copy(Stream &src, Stream &dst, uint16_t max_bytes) {
    uint16_t i;
    for (i = 0; i < max_bytes; i++) {
        if (src.available()) {
            dst.write(src.read());
        }
    }
    return i;
}

// Copy text returning true if the break char is read from src, false otherwise
inline bool stream_copy_breakable(Stream &src, Stream &dst, uint16_t max_bytes, const char break_char) {
    for (uint16_t i = 0; i < max_bytes; i++) {
        if (src.available()) {
            char c = src.read();
            if (c == break_char) {
                return true;
            }
            dst.write(c);
        }
    }
    return false;
}

// Find the index of the value of the property of an object whose name matches the specified string
inline int find_json_prop(const char *json, jsmntok_t *tokens, int num_tokens, int object, const char *prop_name) {
    // Easy way: scan through all tokens looking for parentage
    for (int i = 0; i < num_tokens; i++) {
        jsmntok_t *tok = &tokens[i];
        if (tok->parent == object) {
            if (strlen(prop_name) == tok->end - tok->start &&
                strncmp(json + tok->start, prop_name, (size_t) tok->end - tok->start) == 0) {
                // The next token is the value
                return i + 1;
            }
        }
    }
    return -1;
}


#ifdef _NM_BSP_BIG_END
#define _htonll(m)				(m)
#else
#define _htonll(m)              (uint64_t) ( \
    (((m) & 0xFF00000000000000) >> 56) | \
    (((m) & 0x00FF000000000000) >> 40) | \
    (((m) & 0x0000FF0000000000) >> 24) | \
    (((m) & 0x000000FF00000000) >>  8) | \
    (((m) & 0x00000000FF000000) <<  8) | \
    (((m) & 0x0000000000FF0000) << 24) | \
    (((m) & 0x000000000000FF00) << 40) | \
    (((m) & 0x00000000000000FF) << 56) )
#endif

#define _ntohll                _htonll

// Write hex bytes with a string prefix
inline void hexdump(Stream &out, const char *prefix, const void *mem, const size_t size) {
    out.write(prefix);
    out.write(": ");
    for (size_t i = 0; i < size; i++) {
        out.print((uint8_t) *((const uint8_t *) (mem + i)), HEX);
        out.write(" ");
    }
    out.write("\n");
}

#endif

