#include "term.h"
#include "wifi.h"
#include "cli.h"

void setup() {
  term_init();
  wifi_init();
  cli_init();
}

void loop() {
  wifi_loop();
  cli_loop();
}
