#include "keyboard_test.h"
#include "term.h"

bool keyboard_test() {
    term_writeln("characters you type will be described and echoed in square brackets");
    term_writeln("send break to quit");
    term_writeln("");

    byte c;
    while (term_serial.readBytes(&c, 1) > 0 && c != TERM_BREAK) {
        // Start a new line so the description is clear
        term_writeln("");
        term_write("hex=");
        term_print(c, HEX);
        term_write(", oct=");
        term_print(c, OCT);
        term_write(" dec=");
        term_print(c, DEC);
        term_writeln("");
        term_write(" literal=[");
        term_write(c);
        term_writeln("]");
    }

    term_writeln("keyboard test finished");
    return true;
}
