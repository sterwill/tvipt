#include "keyboard_test.h"
#include "term.h"

bool keyboard_test() {
  term_println("characters you type will be described and echoed in square brackets");
  term_println("send break to quit");
  term_println("");
  
  byte c;
  while (term_serial.readBytes(&c, 1) > 0 && c != '\0') {
    // Start a new line so the description is clear
    term_print("\r");
    term_print("hex=");
    term_print(c, HEX);
    term_print(", oct=");
    term_print(c, OCT);
    term_print(" dec=");
    term_print(c, DEC);
    term_println("");
    term_print(" literal=[");
    term_write(c);
    term_println("]");
  }
  
  term_println("keyboard test finished");
  return true;
}

