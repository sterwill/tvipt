#ifndef _JSON_H
#define _JSON_H

enum json_type {
  json_null,
  json_object,
  json_array,
  json_number,
  json_boolean,
  json_string,
};

struct json_thing {
  json_type type;
  union {
    struct json_object * object_data;
    struct json_array * array_data;
    char * number_data;
    bool boolean_data;
    char * string_data;
  };
};

// Structs that are data for json_types that use structs

struct json_array {
  size_t num_elements;
  struct json_thing * elements;
};

struct json_property {
  char * name;
  struct json_thing value;
};

struct json_object {
  size_t num_props;
  struct json_property * props;
};

struct json_thing * parse(char ** json);

#endif

