// Terminal configuration.

#ifndef _TERM_H
#define _TERM_H

#include <Print.h>
#include <Arduino.h>

#define dbg_serial    Serial
#define term_serial   Serial1

void term_init();
void term_loop();
void term_print(const char * val);
void term_print(long val, int format = DEC);
void term_println(const char * val);
void term_println(long val, int format = DEC);
void term_print_masked(const char * val);
void term_println_masked(const char * val);

#endif

