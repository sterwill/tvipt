#include "term.h"
#include "wifi.h"
#include "cli.h"

#include "config.h"

void setup() {
    term_init();
    wifi_init();
    cli_init();

    // Drain any queued keys (noise?) so we don't put garbage in the command buffer.
    while (term_serial.available()) {
        term_serial.read();
    }

    term_writeln("tvipt/1 (TeleVideo Personal Terminal) Operating System");
    term_writeln("");

    cli_boot(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD, DEFAULT_WIFI_JOIN_TIMEOUT,
             DEFAULT_TELNETS_HOST, DEFAULT_TELNETS_PORT, DEFAULT_TELNETS_USERNAME);
}

void loop() {
    wifi_loop();
    cli_loop();
}
