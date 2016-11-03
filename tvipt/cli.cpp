#include <Arduino.h>
#include "cli.h"
#include "wifi.h"
#include "term.h"

//////////////////////////////////////////////////////////////////////////////
// Commands
//////////////////////////////////////////////////////////////////////////////

#define CMD_CONNECT   "c"
#define CMD_HELP      "h"
#define CMD_HELP2     "help"
#define CMD_INFO      "i"
#define CMD_SCAN      "s"

//////////////////////////////////////////////////////////////////////////////
// Parse Utilities
//////////////////////////////////////////////////////////////////////////////

// Buffers a command until we parse and run it
static char _command[60];
static byte _command_index = 0;

void clear_command() {
  memset(&_command, 0, sizeof(_command));
  _command_index = 0;
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

//////////////////////////////////////////////////////////////////////////////
// Connect
//////////////////////////////////////////////////////////////////////////////

boolean exec_connect(char * tok) {
  char * arg;
  const char * ssid;
  const char * password;

  // Parse SSID
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_ssid);
    return false;
  }
  ssid = arg;

  // Parse password
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_password);
    return false;
  }
  password = arg;

  wifi_connect(ssid, password);
}

//////////////////////////////////////////////////////////////////////////////
// Info
//////////////////////////////////////////////////////////////////////////////

boolean exec_info(char * tok) {
  // Wifi
  
  struct wifi_info w_info;
  wifi_get_info(&w_info);
  
  term_print("wifi status: ");
  term_println(wifi_get_status_description(w_info.status));
  term_print("wifi ssid: ");
  term_println(w_info.ssid);
  term_print("wifi pass: ");
  term_println_masked(w_info.pass);
  term_print("wifi address: ");
  w_info.address.printTo(term_serial);
  term_println("");
  term_print("wifi netmask: ");
  w_info.netmask.printTo(term_serial);
  term_println("");
  term_print("wifi gateway: ");
  w_info.gateway.printTo(term_serial);
  term_println("");

  return true;
}

//////////////////////////////////////////////////////////////////////////////
// Scan
//////////////////////////////////////////////////////////////////////////////

boolean exec_scan(char * tok) {
  wifi_scan();
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// Help
//////////////////////////////////////////////////////////////////////////////

boolean exec_help(char * tok) {
  term_println("c ssid pass   connect to WPA network");
  term_println("h|help        print help");
  term_println("i             print info");
  term_println("s             scan for wireless networks");
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// Command Dispatch
//////////////////////////////////////////////////////////////////////////////

void process_command() {
  boolean ok = false;
  char * tok = NULL;
  const char * command_name = strtok_r(_command, " ", &tok);
  if (strlen(_command) == 0) {
    ok = true;
  } else if (strcmp(command_name, CMD_CONNECT) == 0) {
    ok = exec_connect(tok);
  } else if (strcmp(command_name, CMD_HELP) == 0
    || strcmp(command_name, CMD_HELP2) == 0) {
    ok = exec_help(tok);
  } else if (strcmp(command_name, CMD_INFO) == 0) {
    ok = exec_info(tok);
  } else if (strcmp(command_name, CMD_SCAN) == 0) {
    ok = exec_scan(tok);
  } else {
    term_print(_e_invalid_command);
    term_println(_command);
  }

  if (ok) {
    term_println("= ok");
  } else {
    term_println("= err");
  }
  term_serial.flush();
}

//////////////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////////////

void cli_init() {
  clear_command();
}

void cli_loop() {
  while (term_serial && term_serial.available()) {
    char c = term_serial.read();
    if (ECHO) {
      term_serial.write(c);
    }

    if (c != '\r' && c != '\n' && _command_index < sizeof(_command)) {
      _command[_command_index++] = c;
      continue;
    }

    if (c == '\r' || c == '\n') {
      _command[_command_index] = 0;
      process_command();
    } else {
      term_println("command too long");
      term_println("= err");
    }

    clear_command();
    term_print("> ");
  }
}

