#include <WiFi101.h>
#include "http.h"
#include "term.h"

// Reads one CRLF-terminated HTTP line from the client into a buffer
// and terminates it.  Returns the number of bytes written to buf not 
// including the terminator.
size_t http_read_line(WiFiClient * client, char * buf, size_t bufsize) {
  bool last_char_was_cr = false;

  char * start = buf;
  // end leaves room for the terminator
  char * end = start + bufsize - 1;

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
bool parse_header(char * str, char ** name, char ** value) {
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

void http_request_init(struct http_request * req) {
  req->host = "";
  req->port = 80;
  req->ssl = false;
  req->path_and_query = "";
  req->headers = NULL;
  req->header_cb = NULL;
  req->body_cb = NULL;
  
  req->status = 0;

  req->_client = NULL;
}

void http_get(struct http_request * req) {
  bool connected;
  if (req->ssl) {
    req->_client = new WiFiSSLClient();
    connected = req->_client->connectSSL(req->host, req->port);
  } else {
    req->_client = new WiFiClient();
    connected = req->_client->connect(req->host, req->port);
  }

  if (!connected) {
    req->status = HTTP_STATUS_CONNECT_ERR;
  } else {
    req->_client->print("GET ");
    req->_client->print(req->path_and_query);
    req->_client->println(" HTTP/1.1");
    
    req->_client->print("Host: ");
    req->_client->println(req->host);
   
    req->_client->println("Connection: close");

    req->_client->println("Accept: */*");
    req->_client->println("User-Agent: tvipt/1");
    
    req->_client->println();
    
    req->_client->flush();

    char line[1024];
    size_t read = http_read_line(req->_client, line, sizeof(line));

    // 12 chars is enough for "HTTP/1.1 200"
    if (read < 12 || (strncmp(line, "HTTP/1.0 ", 9) != 0 && strncmp(line, "HTTP/1.1 ", 9) != 0)) {
      req->status = HTTP_STATUS_MALFROMED_RESPONSE_LINE;
      req->_client->stop();
      return;
    }

    req->status = atoi(line + 9);

    // Read headers until we read an empty line
    do {
      // Read headers
      read = http_read_line(req->_client, line, sizeof(line));
      if (read > 0 && req->header_cb != NULL) {
        term_write("first=");
        term_println(line[0], HEX);
        char * header;
        char * value;
        if (!parse_header(line, &header, &value)) {
          req->status = HTTP_STATUS_MALFROMED_RESPONSE_HEADER;
          req->_client->stop();
          return;
        }
        req->header_cb(header, value);
      }
    } while (read > 0);

    // Now read the body
    if (req->body_cb != NULL) {
      req->body_cb(req->_client);
    }
  }

  req->_client->stop();
}

