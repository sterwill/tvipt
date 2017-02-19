#include <WiFi101.h>
#include "http.h"
#include "term.h"

// Reads one CRLF-terminated HTTP line from the client into a buffer
// and terminates it.  Returns the number of bytes written to buf not 
// including the terminator.
size_t http_read_line(WiFiClient *client, char *buf, size_t bufsize) {
    bool last_char_was_cr = false;

    char *start = buf;
    // end leaves room for the terminator
    char *end = start + bufsize - 1;

    while (buf < end && client->connected()) {
        int c = client->read();

        if (c == -1) {
            continue;
        }
        if (c == '\r') {
            last_char_was_cr = true;
            continue;
        }
        if (last_char_was_cr && c == '\n') {
            // Done reading the line or the connection closed
            break;
        }
        last_char_was_cr = false;

        *buf++ = c;
    }

    *buf++ = '\0';
    return (buf - start) - 1;
}

// Mutate a terminated string that contains an HTTP header and get
// pointers to the string parts.  Returns true if the header was parsed
// successfully, false if it wasn't a valid header.
bool parse_header(char *str, char **name, char **value) {
    *name = str;
    while (*str != '\0') {
        if (*str == ':') {
            // If we haven't advanced past the name, then the name would be 
            // empty and that's invalid.
            if (*name == str) {
                return false;
            }

            // Terminate the name part and point to the value after it
            *str = '\0';
            str++;

            // Walk past any leading spaces in the value
            while (*str == ' ') {
                str++;
            }

            *value = str;
            return true;
        }
        str++;
    }

    // Never found a colon
    return false;
}

bool parse_url(struct url_parts *parts, const char *url) {
    enum parse_state {
        in_scheme,
        in_colon_and_slashes,
        in_host,
        in_port,
        in_path_and_query,
    };

    char *scheme = parts->scheme;
    const char *scheme_last = parts->scheme + sizeof(parts->scheme) - 1;
    memset(parts->scheme, '\0', sizeof(parts->scheme));

    char *host = parts->host;
    const char *host_last = parts->host + sizeof(parts->host) - 1;
    memset(parts->host, '\0', sizeof(parts->host));

    char port_buf[6];
    char *port = port_buf;
    const char *port_last = port + sizeof(port_buf) - 1;
    memset(port, '\0', sizeof(port_buf));
    parts->port = 0;

    char *path_and_query = parts->path_and_query;
    const char *path_and_query_last = parts->path_and_query + sizeof(parts->path_and_query) - 1;
    memset(parts->path_and_query, '\0', sizeof(parts->path_and_query));

    parse_state state = in_scheme;

    char c;
    while ((c = *url) != '\0') {
        switch (state) {
            case in_scheme:
                if (c == ':') {
                    state = in_colon_and_slashes;
                } else {
                    // Write if enough room to preserve terminator
                    if (scheme < scheme_last) {
                        *scheme++ = c;
                    }
                    url++;
                }
                break;
            case in_colon_and_slashes:
                if (c == ':' || c == '/') {
                    url++;
                } else {
                    state = in_host;
                }
                break;
            case in_host:
                if (c == ':') {
                    state = in_port;
                    url++;
                } else if (c == '/') {
                    state = in_path_and_query;
                } else {
                    // Write if enough room to preserve terminator
                    if (host < host_last) {
                        *host++ = c;
                    }
                    url++;
                }
                break;
            case in_port:
                if (c == '/') {
                    // If we read anything, convert it
                    if (port > port_buf) {
                        parts->port = atoi(port_buf);
                    }
                    state = in_path_and_query;
                } else {
                    // Write if enough room to preserve terminator
                    if (port < port_last) {
                        *port++ = c;
                    }
                    url++;
                }
                break;
            case in_path_and_query:
                // Write if enough room to preserve terminator
                if (path_and_query < path_and_query_last) {
                    *path_and_query++ = c;
                }
                url++;
                break;
        }
    }

    return state == in_path_and_query;
}

void http_request_init(struct http_request *req) {
    req->host = "";
    req->port = 80;
    req->ssl = false;
    req->path_and_query = "";
    req->headers = NULL;
    req->header_cb = NULL;
    req->body_cb = NULL;

    req->status = 0;

    req->client = NULL;
}

void http_get(struct http_request *req) {
    bool connected;
    if (req->ssl) {
        req->client = new WiFiSSLClient();
        connected = req->client->connectSSL(req->host, req->port);
    } else {
        req->client = new WiFiClient();
        connected = req->client->connect(req->host, req->port);
    }

    if (!connected) {
        req->status = HTTP_STATUS_CONNECT_ERR;
    } else {
        req->client->print("GET ");
        req->client->print(req->path_and_query);
        req->client->println(" HTTP/1.1");

        req->client->print("Host: ");
        req->client->println(req->host);

        req->client->println("Connection: close");

        req->client->println("Accept: */*");
        req->client->println("User-Agent: tvipt/1");

        req->client->println();

        req->client->flush();

        char line[1024];
        size_t read = http_read_line(req->client, line, sizeof(line));

        // 12 chars is enough for "HTTP/1.1 200"
        if (read < 12 || (strncmp(line, "HTTP/1.0 ", 9) != 0 && strncmp(line, "HTTP/1.1 ", 9) != 0)) {
            req->status = HTTP_STATUS_MALFROMED_RESPONSE_LINE;
            req->client->stop();
            return;
        }

        req->status = atoi(line + 9);

        // Read headers until we read an empty line
        do {
            // Read headers
            read = http_read_line(req->client, line, sizeof(line));
            if (read > 0 && req->header_cb != NULL) {
                char *header;
                char *value;
                if (!parse_header(line, &header, &value)) {
                    req->status = HTTP_STATUS_MALFROMED_RESPONSE_HEADER;
                    req->client->stop();
                    return;
                }
                req->header_cb(req, header, value);
            }
        } while (read > 0);

        // Now read the body
        if (req->body_cb != NULL) {
            req->body_cb(req);
        }
    }

    req->client->stop();
}

