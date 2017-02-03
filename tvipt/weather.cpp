#include "weather.h"
#include "http.h"
#include "term.h"
#include "util.h"
#include "jsmn.h"

#define FUTURE_PERIODS 14

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

struct period_forecast {
  char name[24];
  char weather[32];
  char temperature_label[5];
  char temperature[4];
  char precipitation[4];
};

struct weather {
  char timestamp[32];
  char area[32];

  // Current conditions
  char station_id[24];
  char station_name[64];
  char description[24];
  short temperature;
  short dewpoint;
  short relative_humidity;
  short wind_speed;
  short wind_direction;
  short gust;
  char sea_level_pressure[6];
  
  // Future 
  struct period_forecast future[FUTURE_PERIODS];
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

  dbg_serial.println(req.host);
  dbg_serial.println(req.path_and_query);
  
  memset(mapclick_url, '\0', mapclick_url_size);
  http_get(&req);

  dbg_serial.println(req.status, DEC);

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

  dbg_serial.println(req.host);
  dbg_serial.println(req.path_and_query);
  
  memset(mapclick_json, '\0', mapclick_json_size);
  http_get(&req);

  dbg_serial.println(req.status, DEC);

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


char * scopy_json(char * dest, size_t dest_size, const char * json, jsmntok_t * src) {
  // Copy the lesser of the length of the string plus one for the term, or the dest size
  size_t max_chars_to_copy = min(dest_size, (src->end - src->start) +1);
  return scopy(dest, json + src->start, max_chars_to_copy);
}

int atoi_json(const char * json, jsmntok_t * tok) {
  char num_buf[16];
  scopy_json(num_buf, sizeof(num_buf), json, tok);
  return atoi(num_buf);
}

bool parse_mapclick_json(const char * mapclick_json, struct weather * weather) {
  const int tokens_size = 500;
  jsmntok_t tokens[tokens_size];
  jsmn_parser parser;
  
  jsmn_init(&parser);
  int num_tokens = jsmn_parse(&parser, mapclick_json, strlen(mapclick_json), tokens, tokens_size);
  if (num_tokens < 0) {
    term_writeln("Failed to parse the MapClick JSON");
    return false;
  }

  if (num_tokens < 1 || tokens[0].type != JSMN_OBJECT) {
    term_writeln("Top level MapClick item was not an object.");
    return false;
  }

  //////////////////////////////////////////////////////////////////
  // root properties
  
  int timestamp_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "creationDateLocal");
  if (timestamp_i == -1) {
    term_writeln("JSON missing creationDateLocal");
    return false;
  }
  scopy_json(weather->timestamp, sizeof(weather->timestamp), mapclick_json, &tokens[timestamp_i]);

  //////////////////////////////////////////////////////////////////
  // location object
  
  int location_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "location");
  if (location_i == -1) {
    term_writeln("JSON missing location");
    return false;
  }

  int location_area_i = find_json_prop(mapclick_json, tokens, num_tokens, location_i, "areaDescription");
  if (location_area_i == -1) {
    term_writeln("JSON missing location.areaDescription");
    return false;
  }
  scopy_json(weather->area, sizeof(weather->area), mapclick_json, &tokens[location_area_i]);

  //////////////////////////////////////////////////////////////////
  // currentobservation object
  
  int current_observation_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "currentobservation");
  if (current_observation_i == -1) {
    term_writeln("JSON missing currentobservation");
    return false;
  }

  int current_observation_station_id_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "id");
  if (current_observation_station_id_i == -1) {
    term_writeln("JSON missing data.currentobservation.id");
    return false;
  }
  scopy_json(weather->station_id, sizeof(weather->station_id), mapclick_json, &tokens[current_observation_station_id_i]);

  int current_observation_station_name_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "name");
  if (current_observation_station_name_i == -1) {
    term_writeln("JSON missing data.currentobservation.name");
    return false;
  }
  scopy_json(weather->station_name, sizeof(weather->station_name), mapclick_json, &tokens[current_observation_station_name_i]);

  int current_observation_description_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Weather");
  if (current_observation_description_i == -1) {
    term_writeln("JSON missing data.currentobservation.Weather");
    return false;
  }
  scopy_json(weather->description, sizeof(weather->description), mapclick_json, &tokens[current_observation_description_i]);

  int current_observation_temp_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Temp");
  if (current_observation_temp_i == -1) {
    term_writeln("JSON missing data.currentobservation.Temp");
    return false;
  }
  weather->temperature = atoi_json(mapclick_json, &tokens[current_observation_temp_i]);

  int current_observation_dewpoint_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Dewp");
  if (current_observation_dewpoint_i == -1) {
    term_writeln("JSON missing data.currentobservation.Dewp");
    return false;
  }
  weather->dewpoint = atoi_json(mapclick_json, &tokens[current_observation_dewpoint_i]);

  int current_observation_relative_humidity_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Relh");
  if (current_observation_relative_humidity_i == -1) {
    term_writeln("JSON missing data.currentobservation.Relh");
    return false;
  }
  weather->relative_humidity = atoi_json(mapclick_json, &tokens[current_observation_relative_humidity_i]);
  
  int current_observation_wind_speed_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Winds");
  if (current_observation_wind_speed_i == -1) {
    term_writeln("JSON missing data.currentobservation.Winds");
    return false;
  }
  weather->wind_speed = atoi_json(mapclick_json, &tokens[current_observation_wind_speed_i]);

  int current_observation_wind_direction_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Windd");
  if (current_observation_wind_direction_i == -1) {
    term_writeln("JSON missing data.currentobservation.Windd");
    return false;
  }
  weather->wind_direction = atoi_json(mapclick_json, &tokens[current_observation_wind_direction_i]);

  int current_observation_gust_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Gust");
  if (current_observation_gust_i == -1) {
    term_writeln("JSON missing data.currentobservation.Gust");
    return false;
  }
  weather->gust = atoi_json(mapclick_json, &tokens[current_observation_gust_i]);

  int current_observation_sea_level_pressure_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "SLP");
  if (current_observation_sea_level_pressure_i == -1) {
    term_writeln("JSON missing data.currentobservation.SLP");
    return false;
  }
  scopy_json(weather->sea_level_pressure, sizeof(weather->sea_level_pressure), mapclick_json, &tokens[current_observation_sea_level_pressure_i]);

  //////////////////////////////////////////////////////////////////
  // time object
  
  int time_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "time");
  if (time_i == -1) {
    term_writeln("JSON missing time");
    return false;
  }

  int time_period_name_i = find_json_prop(mapclick_json, tokens, num_tokens, time_i, "startPeriodName");
  if (time_period_name_i == -1) {
    term_writeln("JSON missing time.startPeriodName");
    return false;
  }

  int time_temp_label_i = find_json_prop(mapclick_json, tokens, num_tokens, time_i, "tempLabel");
  if (time_temp_label_i == -1) {
    term_writeln("JSON missing tempLabel");
    return false;
  }

  //////////////////////////////////////////////////////////////////
  // data object

  int data_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "data");
  if (data_i == -1) {
    term_writeln("JSON missing data");
    return false;
  }

  int data_temperature_i = find_json_prop(mapclick_json, tokens, num_tokens, data_i, "temperature");
  if (data_temperature_i == -1) {
    term_writeln("JSON missing data.temperature");
    return false;
  }

  int data_pop_i = find_json_prop(mapclick_json, tokens, num_tokens, data_i, "pop");
  if (data_pop_i== -1) {
    term_writeln("JSON missing data.pop");
    return false;
  }

  int data_weather_i = find_json_prop(mapclick_json, tokens, num_tokens, data_i, "weather");
  if (data_weather_i == -1) {
    term_writeln("JSON missing data.weather");
    return false;
  }

  //////////////////////////////////////////////////////////////////
  // future periods (14 of them, pulled from arrays in several objects)
  
  for (int i = 0; i < 14; i++) {
    struct period_forecast * period = &weather->future[i];

    jsmntok_t * name_tok = &tokens[time_period_name_i + 1 + i];
    scopy_json(period->name, sizeof(period->name), mapclick_json, name_tok);

    jsmntok_t * weather_tok = &tokens[data_weather_i + 1 + i];
    scopy_json(period->weather, sizeof(period->weather), mapclick_json, weather_tok);

    jsmntok_t * temperature_tok = &tokens[data_temperature_i + 1 + i];
    scopy_json(period->temperature, sizeof(period->temperature), mapclick_json, temperature_tok);

    jsmntok_t * precipitation_tok = &tokens[data_pop_i + 1 + i];
    if (precipitation_tok->type == JSMN_STRING) {
      scopy_json(period->precipitation, sizeof(period->precipitation), mapclick_json, precipitation_tok);
    }
  }

  return true;
}

const char * wind_direction(int angle) {
  if (angle = 999) {
    return "?";
  }
  
  const char* dirs[]   = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "N" };
  const short angles[] = { 0,   45,   90,  135,  180, 225,  270, 315,  360 };
  
  short min_diff;
  const char * dir_for_min_diff;
  
  // Quick and dirty "find closest direction"
  for (short i = 0; i < sizeof(dirs); i++) {
    short diff = abs(angle - angles[i]);
    if (diff < min_diff) {
      min_diff = diff;
      dir_for_min_diff = dirs[i];
    }
  }

  return dir_for_min_diff;
}

void print_weather(struct weather * weather) {
  term_writeln("");
  
  term_write(weather->area);
  term_write(" (");
  term_write(weather->timestamp);
  term_writeln(")");

  term_write(" Station:      ");
  term_write(weather->station_id);
  term_write(" (");
  term_write(weather->station_name);
  term_writeln(")");

  term_write(" Weather:      ");
  term_writeln(weather->description);

  term_write(" Temperature:  ");
  term_print(weather->temperature, DEC);
  term_writeln(" F");

  term_write(" Humidity:     ");
  term_print(weather->relative_humidity, DEC);
  term_writeln(" %");

  term_write(" Dewpoint:     ");
  term_print(weather->dewpoint, DEC);
  term_writeln(" F");

  term_write(" Pressure:     ");
  term_write(weather->sea_level_pressure);
  term_writeln(" in/Hg");

  term_write(" Wind:         ");
  term_print(weather->wind_speed, DEC);
  term_write(" mph (gusts ");
  term_print(weather->gust, DEC);
  term_write(" mph) from the ");
  term_writeln(wind_direction(weather->wind_direction));

  term_writeln("");

  // 9 printable chars each column, no terminator in this buffer
  char col[9];

  // If the weather starts at tonight, instead of today, today's 
  // high temp cell needs to be blank, and we'll just print 13 of the
  // periods so each day's data remains in the same column.
  bool skip_first_day = strcmp(weather->future[0].name, "Tonight") == 0;
  int periods_to_print = skip_first_day ? 13 : 14;
  int second_name_start = skip_first_day ? 1 : 2;
  int first_day_index = skip_first_day ? 1 : 0;
  int first_night_index = skip_first_day ? 0 : 1;
  
  // Day of week (empty left legend)
  term_write("          Today     ");
  for (int i = second_name_start; i < periods_to_print; i += 2) {
    const char * str = weather->future[i].name;
    // Copy without termination, leaving padding spaces
    memset(col, ' ', sizeof(col));
    memcpy(col, str, min(strlen(str), sizeof(col))); 
    term_write(col, sizeof(col));
    term_write(" ");
  }
  term_writeln("");

  // High temps 
  term_write("high      ");
  if (skip_first_day) {
    term_write("          ");
  }
  for (int i = first_day_index; i < periods_to_print; i += 2) {
    const char * str = weather->future[i].temperature;
    // Copy without termination, leaving padding spaces
    memset(col, ' ', sizeof(col));
    memcpy(col, str, min(strlen(str), sizeof(col))); 
    term_write(col, sizeof(col));
    term_write(" ");
  }
  term_writeln("");

  // Low temps 
  term_write("low       ");
  for (int i = first_night_index; i < periods_to_print; i += 2) {
    const char * str = weather->future[i].temperature;
    // Copy without termination, leaving padding spaces
    memset(col, ' ', sizeof(col));
    memcpy(col, str, min(strlen(str), sizeof(col))); 
    term_write(col, sizeof(col));
    term_write(" ");
  }
  term_writeln("");

  // Daytime precipitation
  term_write("day pcp.  ");
  if (skip_first_day) {
    term_write("          ");
  }
  for (int i = first_day_index; i < periods_to_print; i += 2) {
    const char * str = weather->future[i].precipitation;
    // Copy without termination, leaving padding spaces
    memset(col, ' ', sizeof(col));
    memcpy(col, str, min(strlen(str), sizeof(col))); 
    term_write(col, sizeof(col));
    term_write(" ");
  }
  term_writeln("");

  // Nighttime precipitation
  term_write("nite pcp. ");
  for (int i = first_night_index; i < periods_to_print; i += 2) {
    const char * str = weather->future[i].precipitation;
    // Copy without termination, leaving padding spaces
    memset(col, ' ', sizeof(col));
    memcpy(col, str, min(strlen(str), sizeof(col))); 
    term_write(col, sizeof(col));
    term_write(" ");
  }
  term_writeln("");

  for (int i = 0; i < 5; i++) {
    term_writeln("");
    term_write(weather->future[i].name);
    term_write(": ");
    term_write(weather->future[i].weather);
  }

  term_writeln("");
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
  memset(&weather, 0, sizeof(weather));
  
  if (!parse_mapclick_json(mapclick_json, &weather)) {
    term_writeln("Could not parse the forecast JSON.");
    return;
  }

  print_weather(&weather);
}

