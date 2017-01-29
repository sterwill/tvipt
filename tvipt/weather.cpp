#include "weather.h"
#include "http.h"
#include "term.h"
#include "util.h"
#include "jsmn.h"

struct get_mapclick_url_ctx {
  char * url;
  size_t url_size;
};

struct get_mapclick_data_ctx {
  char * data;
  size_t data_size;

  // Current index where next byte should be written
  char * data_i;
  // Last byte of writable data
  char * data_last;
  size_t data_bytes_read;
};

struct day_forecast {
  short high_temperature;
  char weather[20];
  char text[100];
};

struct weather {
  char timestamp[32];
  char area[24];

  // Current conditions
  char description[20];
  short temperature;
  short dewpoint;
  short relative_humidity;
  short winds;
  short gust;
  
  // Future 
  struct day_forecast future[13];
};

void get_mapclick_url_header_cb(struct http_request * req, const char * header, const char * value) {
  struct get_mapclick_url_ctx * ctx = (struct get_mapclick_url_ctx *) req->caller_ctx;
  
  if (strcmp(header, "Location") == 0) {
    scopy(ctx->url, value, ctx->url_size);
  }
}

boolean get_mapclick_url(const char * zip, char * mapclick_url, size_t mapclick_url_size) {  
  const char base_path_and_query[] = "/zipcity.php?inputstring=";
  
  // Append the zip to the query string; 5 extra bytes for the zip
  char path_and_query[sizeof(base_path_and_query) + 5];
  scopy(path_and_query, base_path_and_query, sizeof(base_path_and_query));
  // strlen doesn't include the terminator; copy 6 from zip to copy its terminator
  scopy(path_and_query + strlen(path_and_query), zip, 6);

  struct get_mapclick_url_ctx ctx;
  ctx.url = mapclick_url;
  ctx.url_size = mapclick_url_size;
  
  struct http_request req;
  http_request_init(&req);
  req.host = "forecast.weather.gov";
  req.path_and_query = path_and_query;
  req.header_cb = get_mapclick_url_header_cb;
  req.body_cb = NULL;
  req.caller_ctx = &ctx;

  memset(mapclick_url, '\0', mapclick_url_size);
  http_get(&req);

  if (req.status != 302) {
    term_write("HTTP error getting MapClick URL: ");
    term_println(req.status, DEC);
    return false;
  }

  if (strlen(mapclick_url) == 0) { 
    term_writeln("Got an empty MapClick URL from the redirect.");
    return false;
  }
  
  return true;
}

void get_mapclick_data_body_cb(struct http_request * req) {
  struct get_mapclick_data_ctx * ctx = (struct get_mapclick_data_ctx *) req->caller_ctx;

  // Read until the request is over or until we reach the penultimate byte
  while (req->client->connected() && ctx->data_i < ctx->data_last) {
    int c = req->client->read();
    if (c != -1) {
      *ctx->data_i++ = c;
      ctx->data_bytes_read++;
    }
  }
}

bool get_mapclick_json(const char * mapclick_url, char * mapclick_json, size_t mapclick_json_size) {
  // Parse the mapclick URL so we can add a query param and query it
  struct url_parts parts;
  if (!parse_url(&parts, mapclick_url)) {
    term_writeln("Could not parse the MapClick URL that was returned: ");
    term_writeln(mapclick_url);
    return false;
  }

  // There are already some query args, so add one more
  char json_path_and_query[256];
  scopy(json_path_and_query, parts.path_and_query, sizeof(json_path_and_query));
  scopy(json_path_and_query + strlen(json_path_and_query), "&FcstType=json", sizeof(json_path_and_query) - strlen(parts.path_and_query));

  struct get_mapclick_data_ctx ctx;
  ctx.data = mapclick_json;
  ctx.data_size = mapclick_json_size;
  ctx.data_i = ctx.data;
  ctx.data_last = ctx.data + ctx.data_size - 1;
  ctx.data_bytes_read = 0;
  
  struct http_request req;
  http_request_init(&req);
  req.host = parts.host;
  if (parts.port != 0) {
    req.port = parts.port;
  }
  req.path_and_query = json_path_and_query;
  req.header_cb = NULL;
  req.body_cb = get_mapclick_data_body_cb;
  req.caller_ctx = &ctx;

  memset(mapclick_json, '\0', mapclick_json_size);
  http_get(&req);

  if (req.status != 200) {
    term_write("HTTP error getting MapClick data: ");
    term_println(req.status, DEC);
    return false;
  }

  if (strlen(mapclick_json) == 0) { 
    term_writeln("Got no MapClick data.");
    return false;
  }

  return true;
}

bool parse_mapclick_json(const char * mapclick_json, struct weather * weather) {
  jsmn_parser parser;
  jsmntok_t tokens[500];
  
  jsmn_init(&parser);
  int num_tokens = jsmn_parse(&parser, mapclick_json, strlen(mapclick_json), tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (num_tokens < 0) {
    term_writeln("Failed to parse the MapClick JSON");
    return false;
  }

  if (num_tokens < 1 || tokens[0].type != JSMN_OBJECT) {
    term_writeln("Top level MapClick item was not an object.");
    return false;
  }

term_println(num_tokens, DEC);
term_println(tokens[0].type, DEC);

  return true;
}

void weather(const char * zip) {
  char mapclick_url[200];
  if (!get_mapclick_url(zip, mapclick_url, sizeof(mapclick_url))) {
    term_writeln("Could not resolve city and state to a location.");
    term_writeln("Was that a valid ZIP code?");
    return;
  }

  char mapclick_json[6000];
  if (!get_mapclick_json(mapclick_url, mapclick_json, sizeof(mapclick_json))) {
    term_writeln("Could not read the forecast data.  This might be a temporary problem.");
    return;
  }

  struct weather weather;
  if (!parse_mapclick_json(mapclick_json, &weather)) {
    term_writeln("Could not parse the forecast JSON.");
    return;
  }
}

