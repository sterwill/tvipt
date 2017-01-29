#include "util.h"
#include "json.h"

void free_object(struct json_object * object) {
}

struct json_object * parse_object(char ** json) {
  struct json_object * object = (struct json_object *) malloc(sizeof(json_object));
  
  return object;
}

void free_array(struct json_array * array) {
}

struct json_array * parse_array(char ** json) {
  struct json_thing * elements[64];
  size_t element_i = 0;

  while (**json == '[' || **json == ',') {
    // Advance past '[' or ','
    *json++;

    // Eats whitespace before the next thing
    struct json_thing * thing = parse(json);
    if (thing == NULL) {
      // Parse error
      return NULL;
    }

    elements[element_i++] = thing;
  }
  
  struct json_array * array = (struct json_array *) malloc(sizeof(json_array));
  array->elements = (struct json_thing *) malloc(sizeof(json_thing) * element_i + 1);
  return array;
}

char * parse_string(char ** json) {
  char str_buf[256];
  char * str = str_buf;
  char * str_last = str + sizeof(str_buf) - 1;
  memset(str, '\0', sizeof(str_buf));

  // TODO deal with embedded quotes
  while (true) {
    char c = **json++;
    if (c != '"') {
      if (str < str_last) {
        *str++ = c;
      }
    } else { 
      break;
    }
  }
  
  return strdup(str_buf);
}

char * parse_number(char ** json) {
  char num_buf[32];
  char * num = num_buf;
  char * num_last = num + sizeof(num_buf) - 1;
  memset(num, '\0', sizeof(num_buf));

  while (true) {
    char c = **json++;
    if (isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
      if (num < num_last) {
        *num++ = c;
      }
    } else {
      break;
    }
  }

  return strdup(num_buf);
}

void finish_literal(char ** json) {
  // Keep reading through "null", "true", and "false"
  while (isalpha(**json)) {
    *json++;
  }
}

void free_thing(struct json_thing * thing) {
  if (thing == NULL) {
    return;
  }
  switch (thing->type) {
    case json_object:
      free_object(thing->object_data);
      break;
    case json_array:
      free_array(thing->array_data);
      break;
    case json_string:
      free(thing->string_data);
      break;
  }
  free(thing);
}

void skip_whitespace(char ** json) {
  char c = **json;
  while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
    *json++;
    c = **json;
  }
}

// Caller must free() result
struct json_thing * parse(char ** json) {
  struct json_thing * thing = (struct json_thing *) malloc(sizeof(struct json_thing));
  
  skip_whitespace(json);
  char c = **json;

  boolean parse_error = false;
  if (c == '{') {
    thing->type = json_object;
    parse_error = (thing->object_data = parse_object(json)) != NULL;
  } else if (c == '[') {
    thing->type = json_array;
    parse_error = (thing->array_data = parse_array(json)) != NULL;
  } else if (c == '"') {
    thing->type = json_string;
    parse_error = (thing->string_data = parse_string(json)) != NULL;
  } else if (c == '-' || isdigit(c)) {
    thing->type = json_string;
    parse_error = (thing->number_data = parse_number(json)) != NULL;
  } else if (c == 't') {
    thing->type = json_boolean;
    thing->boolean_data = true;
    finish_literal(json);
  } else if (c == 'f') {
    thing->type = json_boolean;
    thing->boolean_data = false;
    finish_literal(json);
  } else if (c == 'n') {
    thing->type = json_null;
    finish_literal(json);
  }
  
  if (parse_error) {
    free_thing(thing);
    return NULL;
  }
 
  return thing;
}

