// Utility functions.

#ifndef _UTIL_H
#define _UTIL_H

#include <string.h>

// Like strncpy, but makes sure dest is terminated.
char * scopy(char * dest, const char *src, size_t n) {
  strncpy(dest, src, n - 1);
  if (n > 0) {
    dest[n - 1] = '\0';
  }
  return dest;
}

#endif
