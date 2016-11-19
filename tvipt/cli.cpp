#include <Arduino.h>
#include "cli.h"
#include "wifi.h"
#include "term.h"
#include "tcp.h"
#include "telnets.h"

//////////////////////////////////////////////////////////////////////////////
// Commands
//////////////////////////////////////////////////////////////////////////////

#define CMD_HELP                    "h"
#define CMD_HELP2                   "help"
#define CMD_INFO                    "i"
#define CMD_RESET                   "reset"
#define CMD_TCP_CONNECT             "tcp"
#define CMD_TELNETS_CONNECT         "t"
#define CMD_TELNETS_CONNECT_DEFAULT "o"
#define CMD_WIFI_CONNECT            "c"
#define CMD_WIFI_SCAN               "s"

//////////////////////////////////////////////////////////////////////////////
// Command Executor Return Values
//////////////////////////////////////////////////////////////////////////////

// Command completed successfully; prompt for another
#define RET_OK    0

// Command failed with some error; prompt for another
#define RET_ERR   1

// Command is running and is handling IO
#define RET_IO    2

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
static const char * _e_missing_host = "missing host";
static const char * _e_missing_port = "missing port";

//////////////////////////////////////////////////////////////////////////////
// Help
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_help(char * tok) {
  term_println("c ssid pass     connect to WPA network");
  term_println("h|help          print help");
  term_println("i               print info");
  term_println("o               open Telnet (SSL) connection to the default host");
  term_println("reset           uptime goes to 0");
  term_println("s               scan for wireless networks");
  term_println("tcp host port   open TCP connection");
  term_println("t host port     open Telnet (SSL) connection");
  return RET_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Info
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_info(char * tok) {   
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

  return RET_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Reset
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_reset(char * tok) {
  term_println("starting over!");
  term_serial.flush();
  NVIC_SystemReset();
  
  // Never happens!
  return RET_OK;
}

//////////////////////////////////////////////////////////////////////////////
// TCP Connect
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_tcp_connect(char * tok) {
  char * arg;
  const char * host;
  uint16_t port;
  
  // Parse host
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_host);
    return RET_ERR;
  }
  host = arg;

  // Parse port
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_port);
    return RET_ERR;
  }
  port = atoi(arg);
  
  if (tcp_connect(host, port)) {
    return RET_IO;
  } else { 
    term_println("connection failed");
    return RET_ERR;
  }
}


//////////////////////////////////////////////////////////////////////////////
// Telnet over SSL
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_telnets_connect(char * tok) {
  char * arg;
  const char * host;
  uint16_t port;
  
  // Parse host
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_host);
    return RET_ERR;
  }
  host = arg;

  // Parse port
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_port);
    return RET_ERR;
  }
  port = atoi(arg);
  
  if (telnets_connect(host, port, NULL)) {
    return RET_IO;
  } else { 
    term_println("connection failed");
    return RET_ERR;
  }
}

uint8_t exec_telnets_connect_default(char * tok) {
  if (telnets_connect("tinfig.com", 992, "sterwill")) {
    return RET_IO;
  } else { 
    term_println("connection failed");
    return RET_ERR;
  }
}
//////////////////////////////////////////////////////////////////////////////
// Wifi Connect
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_wifi_connect(char * tok) {
  char * arg;
  const char * ssid;
  const char * password;

  // Parse SSID
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_ssid);
    return RET_ERR;
  }
  ssid = arg;

  // Parse password
  arg = strtok_r(NULL, " ", &tok);
  if (arg == NULL) {
    term_println(_e_missing_password);
    return RET_ERR;
  }
  password = arg;

  wifi_connect(ssid, password);
  return RET_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Wifi Scan
//////////////////////////////////////////////////////////////////////////////

uint8_t exec_wifi_scan(char * tok) {
  wifi_scan();
  return RET_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Command Dispatch
//////////////////////////////////////////////////////////////////////////////

uint8_t process_command() {
  char * tok = NULL;
  const char * command_name = strtok_r(_command, " ", &tok);

  uint8_t ret = RET_ERR;
  if (strlen(_command) == 0) {
    ret = RET_OK;
  } else if (strcmp(command_name, CMD_HELP) == 0
    || strcmp(command_name, CMD_HELP2) == 0) {
    ret = exec_help(tok);
  } else if (strcmp(command_name, CMD_INFO) == 0) {
    ret = exec_info(tok);
  } else if (strcmp(command_name, CMD_RESET) == 0) {
    ret = exec_reset(tok);
  } else if (strcmp(command_name, CMD_TCP_CONNECT) == 0) {
    ret = exec_tcp_connect(tok);
  } else if (strcmp(command_name, CMD_TELNETS_CONNECT) == 0) {
    ret = exec_telnets_connect(tok);
  } else if (strcmp(command_name, CMD_TELNETS_CONNECT_DEFAULT) == 0) {
    ret = exec_telnets_connect_default(tok);
  } else if (strcmp(command_name, CMD_WIFI_CONNECT) == 0) {
    ret = exec_wifi_connect(tok);
  } else if (strcmp(command_name, CMD_WIFI_SCAN) == 0) {
    ret = exec_wifi_scan(tok);
  } else {
    term_print(_e_invalid_command);
    term_println(_command);
  }

  switch (ret) {
    case RET_OK:
      term_println("= ok");
      term_serial.flush();
      break;
    case RET_ERR:
      term_println("= err");
      term_serial.flush();
      break;
    case RET_IO:
      // Print nothing, program is handling IO
      break;
  }
 
  return ret;
}

//////////////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////////////

void cli_init() {
  clear_command();
}

void cli_loop() {
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
      _command[_command_index] = 0;
      prompt = process_command() != RET_IO;
    } else {
      term_println("command too long");
      term_println("= err");
    }

    clear_command();
    if (prompt) {
      term_print("> ");
    }
  }
}

