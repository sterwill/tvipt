// Provides a command line interface for the terminal.

#ifndef _CLI_H
#define _CLI_H

// Echo chars typed
#define ECHO  true

void cli_init();

void cli_loop();

void cli_boot(const char *default_wifi_ssid,
              const char *default_wifi_pass,
              uint16_t wifi_join_timeout,
              const char *default_host,
              uint16_t default_port);

#endif
