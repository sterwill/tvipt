#include "Print.h"

#include "term.h"

void term_init() {
  dbg_serial.begin(19200);
  term_serial.begin(19200);
}

void term_print(const char * val) {
  term_serial.print(val);
}

void term_print(long val, int format) {
  term_serial.print(val, format);
}

void term_println(const char * val) {
  term_serial.print(val);
  term_serial.write('\r');
}

void term_println(long val, int format) {
  term_serial.print(val, format);
  term_serial.write('\r');
}

void term_print_masked(const char * val) {
  while (*val++ != '\0') {
    term_serial.print("*");
  }
}

void term_println_masked(const char * val) {
  term_print_masked(val);
  term_serial.write('\r');  
}

