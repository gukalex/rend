#pragma once

#include "std.h"

buffer http_get(const char* host, int port, const char* req);
buffer http_post(const char* host, int port, const char* req, buffer file);
