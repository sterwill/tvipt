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
inline int find_json_prop(const char *json, jsmntok_t *tokens, size_t num_tokens, int object, const char *prop_name) {
    // Easy way: scan through all tokens looking for parentage
    for (int i = 0; i < num_tokens; i++) {
        jsmntok_t *tok = &tokens[i];
        if (tok->parent == object) {
            if (strlen(prop_name) == tok->end - tok->start &&
                strncmp(json + tok->start, prop_name, tok->end - tok->start) == 0) {
                // The next token is the value
                return i + 1;
            }
        }
    }
    return -1;
}

#endif
