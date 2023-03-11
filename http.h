#pragma once

#include "std.h"

// todo: async interfaces
struct buffer_ex {
    c8* data;
    u64 size;
    u64 capacity;
};
enum http_error { HTTP_OK, HTTP_ERROR };
http_error http_get(const char* host, int port, const char* req, buffer_ex* out);
http_error http_post(const char* host, int port, const char* req, buffer file, buffer_ex* out);

using http_response = void;
enum req_type { REQUEST_GET, REQUEST_POST };
struct server_callback {
    req_type type;
    const char* endpoint;
    void(*callback)(http_response* resp, const char* post_data, u64 post_data_size);
};
void start_server(const char* host, int port, int count, server_callback* callbacks);

// used inside server_callback::callback to fill response data
void http_set_response(http_response* resp, const char* data, u64 size, const char* content_type);
