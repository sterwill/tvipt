#ifndef _TELNETS_H
#define _TELNETS_H

#include <stdint.h>

bool telnets_connect(const char *host, uint16_t port, const char *username);

#endif
