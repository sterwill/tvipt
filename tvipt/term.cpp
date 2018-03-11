#include "Print.h"

#include "term.h"

/*
 * The TeleVideo Personal Terminal (PT) is a typical RS-232 serial terminal 
 * that supports the ASCII character set and a few extended characters.
 * It seems to support a small subset of the escape sequences supported by
 * the 900-series TeleVideo terminals.
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
 * Control Sequences
 * 
 * This table is reproduction of table B-1 in the PT manual.  All of
 * these sequences function as documented on my PT.  Control sequences 
 * are case-insensitive (you can use the lower- or upper-case letter).
 * 
 * Control    Hex   Effect
 * Character  Code
 * 
 * NULL       00    No action
 * CTRL E     05    Send terminal ID
 * CTRL G     07    Sound bell
 * CTRL H     08    Cursor left (BACK SPACE)
 * CTRL I     09    Tabulate (TAB)
 * CTRL J     0A    Cursor down
 * CTRL K     0B    Cursor up
 * CTRL L     0C    Cursor right
 * CTRL M     0D    Carriage return (RETURN)
 * CTRL N     0E    Set DTR control, disable XON/XOFF
 * CTRL O     0F    Set XON/XOFF, disable DTR control
 * CTRL R     12    Transparent print mode on
 * CTRL T     14    Transparent print mode off
 * CTRL Z     1A    Clear screen to spaces
 * CTRL \           CTRL + CLEAR
 * CTRL ^     1E    Home
 * CTRL _     1F    New line
 * CTRL [     1B    Escape sequence initiator
 *   
 * Escape Sequences
 * 
 * Here are the escape sequences that are listed in table B-2 in the
 * PT manual.  Not all of them work on my PT.  I suspect the manual is 
 * for a newer model of the PT or one with newer firmware, or that
 * those functions only work in combination with some other mode
 * or setting that I don't understand.
 * 
 * Escape sequences are case-sensitive.
 *  
 * Sequence       Effect
 * 
 * ESC "          Enable (unlock) keyboard
 * ESC #          Disable (lock) keyboard
 * 
 * ESC )          Half-intensity visual attrbute on (doesn't work)
 * ESC (          Half-intensity visual attrbute off (doesn't work)
 * ESC G 0 m n    Load 0 attribute (m and n unknown)
 * ESC G 1 m n    Load 1 attribute (m and n unknown)
 * ESC F          Activiate 0 attribute mode (doesn't work)
 * ESC H          Activiate 1 attribute mode (doesn't work)
 * 
 * ESC . n        Set cursor appearance (0 <= n <= 4)
 * ESC 1          Set tab stop at cursor position
 * ESC 2          Clear tab stop at cursor position
 * ESC 3          Clear all tab stops
 * 
 * ESC 7          Send screen from home to (but no including) cursor position
 * ESC s          Send entire screen
 * ESC Z 1        Send set up parameters
 * ESC Z 2        Send function key memory
 * 
 * ESC = r c      Move cursor to address
 * ESC I          Tabulate backward
 * 
 * ESC P          Page print (CTRL + PRINT)
 * ESC @          Copy print mode on
 * ESC A          Copy print mode off
 * ESC `          Transparent print mode on
 * ESC a          Transparent print mode off
 * 
 * ESC B          Set block communication mode
 * ESC D H        Set half duplex communication mode
 * ESC D F        Set full duplex communication mode
 * 
 * ESC E          Insert line at cursor position
 * ESC R          Delete cursor line
 * ESC T          Erase to spaces, from (and including) cursor to end of line
 * ESC Y          Erase to spaces, from (and including) cursor to end of page
 * ESC +          Clear screen to spaces
 * ESC *          CTRL + CLEAR
 * 
 * ESC U          Monitor mode on
 * LOC_ESC U
 * LOC ESC X      Monitor mode off
 * ESC V          Self test
 * ESC <unknown>  Power reset (the scanned manual page appears to show a superscript 
 *                hyphen, but I can't find a sequence that does this)
 * 
 * ESC k          Set local edit mode
 * ESC l          Set duplex edit mode
 * ESC 0 1 7      Program SEND key from home to cursor position
 * ESC 0 1 s      Program SEND key for entire screen
 * ESC x 1 n m    Program line delimiter (untested)
 * ESC x 2 n m    Program text delimiter (untested)
 * ESC m x y      Program terminal ID
 * ESC M          Send terminal ID
 * 
 * ESC | n m <name>^<data> CTRL Y   Program function keys, custom state (untested)
 * ESC ] n <name>&<data> CTRL Y     Program directory, phone state (untested)
 * 
 * ESC _          Program EXIT key, starting state
 * ESC ^          Program EXIT key, custom state
 * ESC p          Set 40-column display
 * ESC u          Set 80-column display
 * ESC v          Autowrap mode on
 * ESC w          Autowrap mode off
 * ESC J          Alternate character set on
 * ESC K          Alternate character set off
 * 
 * Bonus Escape Sequences
 *  
 * These aren't listed in the table B-2 in the PT Manual.  I discovered them
 * by trying sequences listed in other TeleVideo terminal manuals.
 * 
 * ESC ~          Reset the terminal to factory settings
 * 
 */

// Contents of /usr/share/tabset/stdcrt from ncurses 5.9.  Not sure if this
// is actually capable of resetting the PT from any weird state.
#define TVIPT_INIT "\x0D\x1B\x33\x0D\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x20\x20\x20\x20\x20\x20\x20\x20\x1B\x31\x0D"
#define TVIPT_CLEAR "\x1a"

void term_init() {
    dbg_serial.begin(115200);
    term_serial.end();
    term_serial.begin(19200);

    while (!term_serial) {}

    term_serial.setTimeout(-1);
    term_serial.write(TVIPT_INIT);
    term_serial.write(TVIPT_CLEAR);
}

void term_clear() {
    term_serial.write(TVIPT_CLEAR);
}

size_t term_write(const char c) {
    return term_serial.write(c);
}

size_t term_write(const uint8_t *buf, size_t size) {
    return term_serial.write(buf, size);
}

size_t term_write(const char *buf, size_t size) {
    return term_serial.write(buf, size);
}

void term_write(const char *val) {
    term_serial.write(val);
}

void term_writeln(const char *val) {
    term_serial.write(val);
    term_serial.write("\r\n");
}

void term_writeln() {
    term_serial.write("\r\n");
}

void term_write_masked(const char *val) {
    while (*val++ != '\0') {
        term_serial.write("*");
    }
}

void term_writeln_masked(const char *val) {
    term_write_masked(val);
    term_serial.write("\r\n");
}

void term_print(long val, int format) {
    term_serial.print(val, format);
}

void term_print(byte row, byte col, char *value) {
    term_print(row, col, value, 0);
}

void term_print(byte row, byte col, char *value, size_t width) {
    term_move(row, col);
    if (width > 0) {
        const char *start = value;
        while (*value != '\0' && value - start < width) {
            term_serial.write(*value++);
        }
    } else {
        term_write(value);
    }
}

void term_println(long val, int format) {
    term_serial.print(val, format);
    term_serial.write("\r\n");
}

int term_readln(char *buf, int max, readln_echo echo) {
    char *start = buf;
    char *end = start + max;
    while (buf < end) {
        int c = term_serial.read();
        if (c != -1) {
            if (c == '\0' || c == '\r' || c == '\n') {
                break;
            }
            if (echo == READLN_ECHO) {
                term_write(c);
            } else if (echo = READLN_MASKED) {
                term_write('*');
            }
            *buf++ = c;
        }
    }
    return buf - start;
}


// Row is 1-24, col is 1-80
void term_move(byte row, byte col) {
    if (row < 1) { row = 1; }
    if (row > 24) { row = 24; }
    if (col < 1) { col = 1; }
    if (col > 80) { col = 80; }

    term_serial.write(TERM_ESCAPE);
    term_serial.write(TERM_MOVE_TO_POS);
    // ASCII 0x20 (SPACE) is row/column value 1, and subsequent ASCII values
    // enumerate the row/column value space up to ASCII 0x6F ('o') for value
    // 80.
    term_serial.write(row + 0x1F);
    term_serial.write(col + 0x1F);
}
