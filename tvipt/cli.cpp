#include <Arduino.h>
#include "cli.h"
#include "wifi.h"
#include "term.h"
#include "tcp.h"
#include "telnets.h"
#include "keyboard_test.h"
#include "weather.h"

//////////////////////////////////////////////////////////////////////////////
// Internal Data
//////////////////////////////////////////////////////////////////////////////

static uint64_t _uptime;
static unsigned long _last_millis = 0;
static bool _handling_io = false;

//////////////////////////////////////////////////////////////////////////////
// Command Executor Return Values
//////////////////////////////////////////////////////////////////////////////

enum command_status {
    // Command has not been run or has not finished
            CMD_UNKNOWN,

    // Command completed successfully; prompt for another
            CMD_OK,

    // Command failed with some error; prompt for another
            CMD_ERR,

    // Command is running and is handling IO  
            CMD_IO
};

//////////////////////////////////////////////////////////////////////////////
// Commands
//////////////////////////////////////////////////////////////////////////////

command_status cmd_chars(char *tok);

command_status cmd_echo(char *tok);

command_status cmd_help(char *tok);

command_status cmd_info(char *tok);

command_status cmd_wifi_join(char *tok);

command_status cmd_keyboard_test(char *tok);

command_status cmd_reset(char *tok);

command_status cmd_wifi_scan(char *tok);

command_status cmd_tcp_connect(char *tok);

command_status cmd_telnets_connect(char *tok);

command_status cmd_weather(char *tok);

struct command {
    const char *name;
    const char *syntax;
    const char *help;

    command_status (*func)(char *tok);
};

// Help is printed in this order
struct command _commands[] = {
        {"chars", "chars [alt]",   "print the (alternate) printable characters", cmd_chars},
        {"echo",  "echo [dbg]",    "echo chars typed to terminal (or debugger)", cmd_echo},
        {"h",     "h",             "print this help",                            cmd_help},
        {"i",     "i",             "print system info",                          cmd_info},
        {"j",     "j",             "join a WPA wireless network",                cmd_wifi_join},
        {"keys",  "keys",          "keyboard input test",                        cmd_keyboard_test},
        {"reset", "reset",         "uptime goes to 0",                           cmd_reset},
        {"scan",  "scan",          "scan for wireless networks",                 cmd_wifi_scan},
        {"tcp",   "tcp host port", "open TCP connection",                        cmd_tcp_connect},
        {"tel",   "tel host port", "open Telnet/SSL connection",                 cmd_telnets_connect},
        {"w",     "w",             "show the weather",                           cmd_weather},
};

//////////////////////////////////////////////////////////////////////////////
// CLI Utilities
//////////////////////////////////////////////////////////////////////////////

// Buffers a command until we parse and run it
static char _command[60];
static byte _command_index = 0;

void clear_command() {
    memset(&_command, 0, sizeof(_command));
    _command_index = 0;
}

void print_prompt() {
    term_write("> ");
}

uint8_t parse_uint8(char *str, uint8_t *dest) {
    char *endptr = 0;
    uint8_t value = (uint8_t) strtol(str, &endptr, 10);
    // The strtol man pages says this signifies success
    if (*str != 0 && *endptr == 0) {
        *dest = value;
        return 1;
    }
    return 0;
}

uint16_t parse_uint16(char *str, uint16_t *dest) {
    char *endptr = 0;
    uint16_t value = (uint16_t) strtol(str, &endptr, 10);
    // The strtol man pages says this signifies success
    if (*str != 0 && *endptr == 0) {
        *dest = value;
        return 1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Error Strings
//////////////////////////////////////////////////////////////////////////////

static const char *_e_missing_ssid = "missing ssid";
static const char *_e_missing_password = "missing password";
static const char *_e_invalid_command = "invalid command: ";
static const char *_e_missing_host = "missing host";
static const char *_e_missing_port = "missing port";
static const char *_e_invalid_target = "invalid target";
static const char *_e_invalid_charset = "invalid charset: ";
static const char *_e_missing_zip = "missing zip";

//////////////////////////////////////////////////////////////////////////////
// Chars
//////////////////////////////////////////////////////////////////////////////

#define FIRST_PRINTABLE   0x20
#define LAST_PRINTABLE    0x7E

command_status cmd_chars(char *tok) {
    char *arg;
    boolean print_alt_chars = false;

    // Parse charset
    arg = strtok_r(NULL, " ", &tok);
    if (arg != NULL) {
        if (strcmp("alt", arg) == 0) {
            print_alt_chars = true;
        } else {
            term_write(_e_invalid_charset);
            term_writeln(arg);
            return CMD_ERR;
        }
    }

    // 94 total printable chars, printed 8 per line in columns 10 chars wide; 12 lines total  
    byte c = FIRST_PRINTABLE;
    while (c <= FIRST_PRINTABLE + 11) {
        for (int col = 0; col < 8; col++) {
            if (term_serial.read() == TERM_XOFF) {
                while (term_serial.read() != TERM_XON) {}
            }

            byte ch = (byte) (c + (col * 12));

            // On the last row, we'll have some columns to leave empty
            if (ch > LAST_PRINTABLE) {
                break;
            }

            term_write("0x");
            term_print(ch, HEX);
            term_write(" ");

            if (print_alt_chars) {
                term_write(TERM_ESCAPE);
                term_write(TERM_ENABLE_ALT_CHAR);
            }

            term_write(ch);

            if (print_alt_chars) {
                term_write(TERM_ESCAPE);
                term_write(TERM_DISABLE_ALT_CHAR);
            }

            term_write("    ");
        }
        c++;
        term_writeln("");
    }

    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Echo
//////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>

command_status cmd_echo(char *tok) {
    char *arg;
    Print *target = &term_serial;

    // Parse target
    arg = strtok_r(NULL, " ", &tok);
    if (arg != NULL) {
        if (strcmp("dbg", arg) == 0) {
            target = &dbg_serial;
        } else {
            term_write(_e_invalid_target);
            term_writeln(arg);
            return CMD_ERR;
        }
    }

    term_writeln("send break to quit");

    while (true) {
        // Handle break and flow control
        int c = term_serial.read();
        if (c == TERM_BREAK) {
            return CMD_OK;
        } else if (c != -1) {
            target->write((uint8_t) c);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Help
//////////////////////////////////////////////////////////////////////////////

command_status cmd_help(char *tok) {
    // Measure syntax column for padding
    uint8_t max_syntax_width = 0;
    for (int i = 0; i < (sizeof(_commands) / sizeof(struct command)); i++) {
        max_syntax_width = (uint8_t) max(max_syntax_width, strlen(_commands[i].syntax));
    }

    for (int i = 0; i < (sizeof(_commands) / sizeof(struct command)); i++) {
        // Syntax with padding
        term_write(_commands[i].syntax, strlen(_commands[i].syntax));
        for (int j = strlen(_commands[i].syntax); j < max_syntax_width; j++) {
            term_write(' ');
        }

        // Separator
        term_write("    ");

        // Help column (might wrap if really long)
        term_writeln(_commands[i].help);
    }

    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Info
//////////////////////////////////////////////////////////////////////////////

#define SECOND_MILLIS (1000)
#define MINUTE_MILLIS (60 * SECOND_MILLIS)
#define HOUR_MILLIS   (60 * MINUTE_MILLIS)
#define DAY_MILLIS    (24 * HOUR_MILLIS)

void print_time(uint64_t elapsed_ms) {
    uint16_t days = (uint16_t) (elapsed_ms / DAY_MILLIS);
    term_print(days, DEC);
    term_write(" days, ");
    elapsed_ms -= days * DAY_MILLIS;

    uint8_t hours = (uint8_t) (elapsed_ms / HOUR_MILLIS);
    term_print(hours, DEC);
    term_write(" hours, ");
    elapsed_ms -= hours * HOUR_MILLIS;

    uint8_t minutes = (uint8_t) (elapsed_ms / MINUTE_MILLIS);
    term_print(minutes, DEC);
    term_write(" minutes, ");
    elapsed_ms -= minutes * MINUTE_MILLIS;

    uint8_t seconds = (uint8_t) (elapsed_ms / SECOND_MILLIS);
    term_print(seconds, DEC);
    term_write(" seconds, ");
    elapsed_ms -= seconds * SECOND_MILLIS;

    term_print((long) elapsed_ms, DEC);
    term_write(" milliseconds");
}

command_status cmd_info(char *tok) {
    // System
    term_write("uptime: ");
    print_time(_uptime);
    term_writeln("");

    // Wifi

    struct wifi_info w_info;
    wifi_get_info(&w_info);

    term_write("wifi status: ");
    term_writeln(w_info.status_description);

    term_write("wifi ssid: ");
    term_writeln(w_info.ssid);

    term_write("wifi pass: ");
    term_writeln_masked(w_info.pass);

    term_write("wifi address: ");
    w_info.address.printTo(term_serial);
    term_writeln("");

    term_write("wifi netmask: ");
    w_info.netmask.printTo(term_serial);
    term_writeln("");

    term_write("wifi gateway: ");
    w_info.gateway.printTo(term_serial);
    term_writeln("");

    term_write("wifi time: ");
    term_print(w_info.time, DEC);
    term_writeln("");

    term_write("wifi firmware: ");
    term_writeln(w_info.firmware_version);

    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Keyboard Test
//////////////////////////////////////////////////////////////////////////////

command_status cmd_keyboard_test(char *tok) {
    if (keyboard_test()) {
        return CMD_IO;
    } else {
        term_writeln("keyboard test failed");
        return CMD_ERR;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Reset
//////////////////////////////////////////////////////////////////////////////

command_status cmd_reset(char *tok) {
    term_writeln("starting over!");
    term_serial.flush();
    while (term_serial.available()) {
        term_serial.read();
    }
    NVIC_SystemReset();

    // Never happens!
    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// TCP Connect
//////////////////////////////////////////////////////////////////////////////

command_status cmd_tcp_connect(char *tok) {
    char *arg;
    const char *host;
    uint16_t port;

    // Parse host
    arg = strtok_r(NULL, " ", &tok);
    if (arg == NULL) {
        term_writeln(_e_missing_host);
        return CMD_ERR;
    }
    host = arg;

    // Parse port
    arg = strtok_r(NULL, " ", &tok);
    if (arg == NULL) {
        term_writeln(_e_missing_port);
        return CMD_ERR;
    }
    port = (uint16_t) atoi(arg);

    if (tcp_connect(host, port)) {
        return CMD_IO;
    } else {
        term_writeln("connection failed");
        return CMD_ERR;
    }
}


//////////////////////////////////////////////////////////////////////////////
// Telnet over SSL
//////////////////////////////////////////////////////////////////////////////

command_status cmd_telnets_connect(char *tok) {
    char *arg;
    const char *host;
    uint16_t port;

    // Parse host
    arg = strtok_r(NULL, " ", &tok);
    if (arg == NULL) {
        term_writeln(_e_missing_host);
        return CMD_ERR;
    }
    host = arg;

    // Parse port
    arg = strtok_r(NULL, " ", &tok);
    if (arg == NULL) {
        term_writeln(_e_missing_port);
        return CMD_ERR;
    }
    port = (uint16_t) atoi(arg);

    if (telnets_connect(host, port, NULL)) {
        return CMD_IO;
    } else {
        term_writeln("connection failed");
        return CMD_ERR;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Wifi Join
//////////////////////////////////////////////////////////////////////////////

command_status cmd_wifi_join(char *tok) {
    int n;
    char ssid[100];
    char pass[100];

    term_write("ssid: ");
    n = term_readln(ssid, sizeof(ssid) - 1, READLN_ECHO);
    if (n == 0) {
        term_writeln(_e_missing_ssid);
        return CMD_ERR;
    }
    ssid[n] = '\0';
    term_writeln("");

    term_write("password: ");
    n = term_readln(pass, sizeof(pass) - 1, READLN_MASKED);
    pass[n] = '\0';
    term_writeln("");

    wifi_connect(ssid, pass);
    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Wifi Scan
//////////////////////////////////////////////////////////////////////////////

void print_wifi_network(struct wifi_network net) {
    term_write("\"");
    term_write(net.ssid);
    term_write("\" ");
    term_print(net.rssi, DEC);
    term_write(" dBm, ");
    term_writeln(net.encryption_description);
}

command_status cmd_wifi_scan(char *tok) {
    int nets = wifi_scan(print_wifi_network);
    if (nets == -1) {
        term_writeln("scan error");
        return CMD_ERR;
    } else {
        term_print(nets, DEC);
        term_writeln(" networks");
        return CMD_OK;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Weather
//////////////////////////////////////////////////////////////////////////////

command_status cmd_weather(char *tok) {
    int n;
    char zip[6];

    term_write("zip: ");
    n = term_readln(zip, sizeof(zip) - 1, READLN_ECHO);
    if (n == 0) {
        term_writeln(_e_missing_zip);
        return CMD_ERR;
    }
    zip[n] = '\0';
    term_writeln("");

    weather(zip);
    return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Command Dispatch
//////////////////////////////////////////////////////////////////////////////

command_status process_command() {
    char *tok = NULL;
    const char *command_name = strtok_r(_command, " ", &tok);

    command_status status = CMD_UNKNOWN;

    if (strlen(_command) == 0) {
        status = CMD_OK;
    } else {
        // Find the matching command by name
        for (int i = 0; i < (sizeof(_commands) / sizeof(struct command)); i++) {
            if (strcmp(command_name, _commands[i].name) == 0) {
                status = _commands[i].func(tok);
                break;
            }
        }

        // No name matched
        if (status == CMD_UNKNOWN) {
            term_write(_e_invalid_command);
            term_writeln(_command);
            status = CMD_ERR;
        }
    }

    switch (status) {
        case CMD_OK:
            term_writeln("= ok");
            term_serial.flush();
            break;
        case CMD_ERR:
            term_writeln("= err");
            term_serial.flush();
            break;
        case CMD_IO:
            // Print nothing, program is handling IO
            break;
        default:
            break;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////////////

void cli_init() {
    clear_command();
}

void cli_loop() {
    // Increase uptime
    unsigned long now = millis();
    _uptime += millis() - _last_millis;
    _last_millis = now;

    // Don't consume keys if commands we're running are handling IO
    if (wifi_has_loop_callback()) {
        _handling_io = true;
        return;
    }

    // If we just stopped handling IO, force a prompt
    if (_handling_io) {
        _handling_io = false;
        print_prompt();
    }

    while (term_serial && term_serial.available()) {
        uint8_t c = (uint8_t) term_serial.read();

        // Handle backspace before normal character echo so we can do what's
        // required to make it look right and prevent it from erasing too far
        // back.
        if (c == 0x08) {
            if (_command_index > 0) {
                if (ECHO) {
                    // Go back one char
                    term_serial.write(0x08);
                    // Overwrite char with a space
                    term_serial.write(" ");
                    // Go back again
                    term_serial.write(0x08);
                }
                // Shrink the command buffer
                _command[_command_index] = 0;
                _command_index--;
            }
            continue;
        }

        if (ECHO) {
            term_serial.write(c);
        }

        if (c != '\r' && c != '\n' && _command_index < sizeof(_command)) {
            _command[_command_index++] = c;
            continue;
        }

        boolean prompt = true;
        if (c == '\r' || c == '\n') {
            term_writeln("");
            _command[_command_index] = 0;
            prompt = process_command() != CMD_IO;
        } else {
            term_writeln("command too long");
            term_writeln("= err");
        }

        clear_command();
        if (prompt) {
            print_prompt();
        }
    }
}

void cli_boot(const char *default_wifi_ssid,
              const char *default_wifi_pass,
              uint16_t wifi_join_timeout,
              const char *default_telnets_host,
              uint16_t default_telnets_port,
              const char *default_telnets_user) {

    boolean connected = false;
    if (default_wifi_ssid != NULL && default_wifi_pass != NULL) {
        term_write("wifi: auto join ssid=[");
        term_write(default_wifi_ssid);
        term_write("] timeout=");
        term_print(wifi_join_timeout, DEC);
        term_writeln("ms");

        wifi_connect(default_wifi_ssid, default_wifi_pass);
        unsigned long timeout = millis() + wifi_join_timeout;
        while (!wifi_is_connected() && millis() < timeout) {
            delay(1);
        }

        if (default_telnets_host != NULL && default_telnets_port > 0) {
            term_write("telnets: auto connect host=");
            term_write(default_telnets_host);
            term_write(" port=");
            term_print(default_telnets_port, DEC);
            term_writeln("");

            connected = telnets_connect(default_telnets_host, default_telnets_port, default_telnets_user);
        }
    }

    if (!connected) {
        print_prompt();
    }
}
