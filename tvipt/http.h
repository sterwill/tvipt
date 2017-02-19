#ifndef _HTTP_H
#define _HTTP_H

#include <WiFi101.h>

#define HTTP_STATUS_CONNECT_ERR                 -1
#define HTTP_STATUS_MALFROMED_RESPONSE_LINE     -2
#define HTTP_STATUS_MALFROMED_RESPONSE_HEADER   -3

enum http_method_state {
    HTTP_METHOD_NEW,
    HTTP_METHOD_READING_RESPONSE_HEADERS,
    HTTP_METHOD_READING_RESPONSE_BODY,
    HTTP_METHOD_DONE,
    HTTP_METHOD_CLOSED,
};

struct http_request {
    // Caller fills these fields
    const char *host;
    uint16_t port;
    bool ssl;
    const char *path_and_query;
    struct http_key_value **headers;

    void (*header_cb)(struct http_request *req, const char *header, const char *value);

    void (*body_cb)(struct http_request *req);

    void *caller_ctx;

    // HTTP methods fill these fields
    int status;

    // Valid during callback execution
    WiFiClient *client;
};

struct url_parts {
    char scheme[6];
    char host[64];
    uint16_t port;
    char path_and_query[256];
};

bool parse_url(struct url_parts *parts, const char *url);

void http_request_init(struct http_request *req);

void http_get(struct http_request *req);

#endif
