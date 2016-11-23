#include "Print.h"

#include "term.h"

/*
 * The TeleVideo Personal Terminal (PT) is a typical RS-232 serial terminal 
 * that supports the ASCII character set (and possibly some extended 
 * character sets, I haven't probed around much).  The control characters 
 * don't seem to be compatible with more common terminals like the DEC VT 
 * series, and it has a few quirks of its own.
 * 
 * Newlines
 * 
 * The terminal always sends ASCII CR when the RETURN key is pressed.
 * The menu setting SETUP > MISC > (CR RETURN|CRLF RETURN) configures how
 * the terminal interprets newlines that it prints to the screen, but it 
 * doesn't affect keyboard operation.
 * 
 * tvipt requires the terminal to be set to "CR RETURN" mode.  "CR RETURN" 
 * mode seems to cause the terminal to handle a received CR by moving the 
 * cursor down one line.  In "CRLF RETURN" mode the terminal seems to handle
 * a received CR like it was a CR and LF, and both move the cursor down one 
 * line and set it to the left-most column.  In "CR RETURN" mode, we have 
 * to send our own LFs after CRs.  This is what the "tvipt" terminal type
 * expects to have to do on Linux, so the tvipt system uses this convention.
 * 
 * Backspace / Delete
 * 
 * The terminal sends ASCII BS when BACK SPACE is pressed and ASCII DEL
 * when DEL is pressed.  This is normal and good.
 * 
 */
 
void term_init() {
  dbg_serial.begin(19200);
  term_serial.begin(19200);
  term_serial.setTimeout(-1);
}

size_t term_write(const char c) {
  return term_serial.write(c);
}

size_t term_write(const uint8_t * buf, size_t size) {
  return term_serial.write(buf, size);
}

size_t term_write(const char * buf, size_t size) {
  return term_serial.write(buf, size);
}

void term_write(const char * val) {
  term_serial.write(val);
}

void term_writeln(const char * val) {
  term_serial.write(val);
  term_serial.write("\r\n");
}

void term_write_masked(const char * val) {
  while (*val++ != '\0') {
    term_serial.write("*");
  }
}

void term_writeln_masked(const char * val) {
  term_write_masked(val);
  term_serial.write("\r\n");
}

void term_print(long val, int format) {
  term_serial.print(val, format);
}

void term_println(long val, int format) {
  term_serial.print(val, format);
  term_serial.write("\r\n");
}

