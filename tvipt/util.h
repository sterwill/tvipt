// Utility functions.

#ifndef _UTIL_H
#define _UTIL_H

#include <string.h>

// Like strncpy, but makes sure dest is terminated.
inline char * scopy(char * dest, const char *src, size_t n) {
  strncpy(dest, src, n - 1);
  if (n > 0) {
    dest[n - 1] = '\0';
  }
  return dest;
}

inline uint16_t stream_copy(Stream & src, Stream & dst, uint16_t max_bytes) {
  uint16_t i;
  for (i = 0; i < max_bytes; i++) {
    if (src.available()) {
      dst.write(src.read());
    }
  }
  return i;
}

#endif
