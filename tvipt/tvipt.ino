#include "term.h"
#include "wifi.h"
#include "cli.h"

void setup() {
  term_init();
  wifi_init();
  cli_init();
  
  term_clear();
  term_writeln("tvipt/1 (TeleVideo Personal Terminal) Operating System");
  term_writeln("");
  
  // Set the boot-time defaults here
  //cli_boot("My Wifi Net", "p455w0rd", 5000, "server.example.com", 992, "myuser");
  cli_boot(NULL, NULL, 0, NULL, 0, NULL);
}

void loop() {
  wifi_loop();
  cli_loop();
}
