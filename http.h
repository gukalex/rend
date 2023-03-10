#pragma once

#include "std.h"

buffer http_get(const char* host, int port, const char* req);
buffer http_post(const char* host, int port, const char* req, buffer file);

enum struct req_type { GET, POST };
struct server_callback {
    req_type type;
    const char* endpoint;
    void(*callback)(char*);
};

void start_server(const char* host, int port, int count, server_callback* callbacks);
