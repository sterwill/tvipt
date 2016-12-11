#include <Arduino.h>
#include "cli.h"
#include "wifi.h"
#include "term.h"
#include "tcp.h"
#include "telnets.h"
#include "keyboard_test.h"

//////////////////////////////////////////////////////////////////////////////
// Internal Data
//////////////////////////////////////////////////////////////////////////////

static uint64_t _uptime;
static unsigned long _last_millis = 0;

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

command_status cmd_chars(char * tok);
command_status cmd_help(char * tok);
command_status cmd_info(char * tok);
command_status cmd_wifi_join(char * tok);
command_status cmd_keyboard_test(char * tok);
command_status cmd_reset(char * tok);
command_status cmd_wifi_scan(char * tok);
command_status cmd_tcp_connect(char * tok);
command_status cmd_telnets_connect(char * tok);

struct command {
  const char * name;
  const char * syntax;
  const char * help;
  command_status (* func)(char * tok);
};

// Help is printed in this order
struct command _commands[] = {
  {"chars", "chars",          "print all the printable characters",               cmd_chars},
  {"h",     "h",              "print this help",                                  cmd_help},
  {"i",     "i",              "print system info",                                cmd_info},
  {"j",     "j ssid pass",    "join a WPA wireless network",                      cmd_wifi_join},
  {"keys",  "keys",           "keyboard input test",                              cmd_keyboard_test},
  {"reset", "reset",          "uptime goes to 0",                                 cmd_reset},
  {"scan",  "scan",           "scan for wireless networks",                       cmd_wifi_scan},
  {"tcp",   "tcp host port",  "open TCP connection",                              cmd_tcp_connect},
  {"tel",   "tel host port",  "open Telnet/SSL connection",                       cmd_telnets_connect},
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

uint8_t parse_uint8(char * str, uint8_t * dest) {
  char * endptr = 0;
  uint8_t value = strtol(str, &endptr, 10);
  // The strtol man pages says this signifies success
  if (*str != 0 && *endptr == 0) {
    *dest = value;
    return 1;
  }
  return 0;
}

uint16_t parse_uint16(char * str, uint16_t * dest) {
  char * endptr = 0;
  uint16_t value = strtol(str, &endptr, 10);
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

static const char * _e_missing_ssid = "missing ssid";
static const char * _e_missing_password = "missing password";
static const char * _e_invalid_command = "invalid command: ";
static const char * _e_missing_host = "missing host";
static const char * _e_missing_port = "missing port";

//////////////////////////////////////////////////////////////////////////////
// Chars
//////////////////////////////////////////////////////////////////////////////

#define FIRST_PRINTABLE   32
#define LAST_PRINTABLE    127

command_status cmd_chars(char * tok) {
  term_writeln("send break to quit");
  
  byte i = FIRST_PRINTABLE;
  boolean transmit = true;

  while (true) {
    if (i > LAST_PRINTABLE) {
      i = FIRST_PRINTABLE;
    }

    // Handle break and flow control
    switch (term_serial.read()) {
      case TERM_BREAK:
        return CMD_OK;
      case TERM_XOFF:
        transmit = false;
        break;
      case TERM_XON:
        transmit = true;
        break;
    }
    
    if (transmit) {
      term_write(i);
      i++;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// Help
//////////////////////////////////////////////////////////////////////////////

command_status cmd_help(char * tok) {
  // Measure syntax column for padding
  uint8_t max_syntax_width = 0;
  for (int i = 0; i < (sizeof(_commands) / sizeof(struct command)); i++) {
    max_syntax_width = max(max_syntax_width, strlen(_commands[i].syntax));
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
  uint16_t days = elapsed_ms / DAY_MILLIS;
  term_print(days, DEC);
  term_write(" days, ");
  elapsed_ms -= days * DAY_MILLIS;

  uint8_t hours = elapsed_ms / HOUR_MILLIS;
  term_print(hours, DEC);
  term_write(" hours, ");
  elapsed_ms -= hours * HOUR_MILLIS;

  uint8_t minutes = elapsed_ms / MINUTE_MILLIS;
  term_print(minutes, DEC);
  term_write(" minutes, ");
  elapsed_ms -= minutes * MINUTE_MILLIS;

  uint8_t seconds = elapsed_ms / SECOND_MILLIS;
  term_print(seconds, DEC);
  term_write(" seconds, ");
  elapsed_ms -= seconds * SECOND_MILLIS;

  term_print(elapsed_ms, DEC);
  term_write(" milliseconds");
}

command_status cmd_info(char * tok) {   
  // System
  term_write("uptime: ");
  print_time(_uptime);
  term_writeln("");
  
  // Wifi
  
  struct wifi_info w_info;
  wifi_get_info(&w_info);
  
  term_write("wifi status: ");
  term_writeln(wifi_get_status_description(w_info.status));
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

  return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Keyboard Test
//////////////////////////////////////////////////////////////////////////////

command_status cmd_keyboard_test(char * tok) {
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

command_status cmd_reset(char * tok) {
  term_writeln("starting over!");
  term_serial.flush();
  NVIC_SystemReset();
  
  // Never happens!
  return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// TCP Connect
//////////////////////////////////////////////////////////////////////////////

command_status cmd_tcp_connect(char * tok) {
  char * arg;
  const char * host;
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
  port = atoi(arg);
  
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

command_status cmd_telnets_connect(char * tok) {
  char * arg;
  const char * host;
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
  port = atoi(arg);
  
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

command_status cmd_wifi_join(char * tok) {
  char * arg;
  const char * ssid;
  const char * password;

  // Parse SSID
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_writeln(_e_missing_ssid);
    return CMD_ERR;
  }
  ssid = arg;

  // Parse password
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_writeln(_e_missing_password);
    return CMD_ERR;
  }
  password = arg;

  wifi_connect(ssid, password);
  return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Wifi Scan
//////////////////////////////////////////////////////////////////////////////

command_status cmd_wifi_scan(char * tok) {
  wifi_scan();
  return CMD_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Command Dispatch
//////////////////////////////////////////////////////////////////////////////

command_status process_command() {
  char * tok = NULL;
  const char * command_name = strtok_r(_command, " ", &tok);

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
    return;
  }
  
  while (term_serial && term_serial.available()) {
    char c = term_serial.read();
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

void cli_boot(const char * default_wifi_ssid, 
              const char * default_wifi_pass,
              uint16_t wifi_join_timeout,
              const char * default_telnets_host, 
              uint16_t default_telnets_port, 
              const char * default_telnets_user) {

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
