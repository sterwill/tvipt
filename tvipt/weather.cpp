#include "weather.h"
#include "http.h"
#include "term.h"
#include "util.h"

void print_body(Client * client) {
  while (client->connected()) {
    int c = client->read();
    if (c != -1) {
      term_write(c);
    }
  }
}

void print_header(const char * header, const char * value) {
  term_write("h=");
  term_writeln(header);
  term_write("v=");
  term_writeln(value);  
}

boolean get_lat_lon(const char * zip, char lat[], char lon[]) {  
  const char base_path_and_query[] = "/zipcity.php?inputstring=";
  
  // Append the zip to the query string; 5 extra bytes for the zip
  char path_and_query[sizeof(base_path_and_query) + 5];
  scopy(path_and_query, base_path_and_query, sizeof(base_path_and_query));
  // strlen doesn't include the terminator; copy 6 from zip to copy its terminator
  scopy(path_and_query + strlen(path_and_query), zip, 6);

  struct http_request req;
  http_request_init(&req);
  req.host = "forecast.weather.gov";
  req.path_and_query = path_and_query;
  req.header_cb = print_header;
  req.body_cb = print_body;
  
  http_get(&req);

  term_write("status: ");
  term_println(req.status, DEC);

  return true;
}

void weather(const char * zip) {
  char lat[20];
  char lon[20];
  
  if (!get_lat_lon(zip, lat, lon)) {
    term_writeln("Could not resolve city and state to a location.");
    return;
  }
}

