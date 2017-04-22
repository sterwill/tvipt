#include "think.h"
#include "http.h"
#include "term.h"
#include "util.h"

struct get_color_ctx {
    String response;
};

void get_color_body_cb(struct http_request *req) {
    struct get_color_ctx *ctx = (struct get_color_ctx *) req->caller_ctx;

    String line;
    while (req->client->available()) {
        line = req->client->readStringUntil('\r');
    }
    line.trim();

    ctx->response = line;
}

void think(const char *color) {
    char path_and_query[80];
    strcpy(path_and_query, "/?color=");
    scopy(path_and_query + strlen(path_and_query), color, strlen(color) + 1);

    struct get_color_ctx ctx;

    struct http_request req;
    http_request_init(&req);
    req.host = "tinfig.com";
    req.port = 5000;
    req.path_and_query = path_and_query;
    req.header_cb = NULL;
    req.body_cb = get_color_body_cb;
    req.caller_ctx = &ctx;

    http_get(&req);

    dbg_serial.println(req.status, DEC);

    if (req.status != 200) {
        term_write("HTTP error setting color: ");
        term_println(req.status, DEC);
        return;
    }

    if (ctx.response.length() == 0) {
        term_writeln("Got an empty response.");
        return;
    }

    term_writeln(ctx.response.c_str());
}

