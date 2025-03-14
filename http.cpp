#include "http.h"
#include "httplib.h"
#include <stdlib.h> // memcpy
#include <thread>
#include "std.h"

http_error http_get(const char* host, int port, const char* req, buffer_ex* out) {
    httplib::Client cli(host, port);
    auto res = cli.Get(req);
    if (res) {
        u64 min_size = MIN(out->capacity, res->body.size());
        out->size = res->body.size();
        memcpy(out->data, res->body.data(), min_size);
    }
    return (res.error() == httplib::Error::Success ? HTTP_OK: HTTP_ERROR);
}

http_error http_post(const char* host, int port, const char* req, buffer file, buffer_ex* out) {
    httplib::Client cli(host, port);
    auto res = cli.Post(req, 
        {{"name", std::string((c8*)file.data, file.size), "filename", "application/octet-stream"}}
    );
    if (res) {
        u64 min_size = MIN(out->capacity, res->body.size());
        out->size = res->body.size();
        memcpy(out->data, res->body.data(), min_size);
    }
    return (res.error() == httplib::Error::Success ? HTTP_OK : HTTP_ERROR);
}

void start_server(const char* host, int port, int count, server_callback *callbacks) {
    std::thread srv_thread([=]() {
        httplib::Server srv;
        for (int i = 0; i < count; i++) {
            switch (callbacks[i].type) {
            case REQUEST_GET:
                srv.Get(callbacks[i].endpoint, [=](const httplib::Request& req, httplib::Response& res) {
                    {
                        print("remote addr %s (%d), local addr %s (%d)", req.remote_addr.c_str(), req.remote_port, req.local_addr.c_str(), req.local_port);
                    }
                    switch (callbacks[i].callback_type) {
                    case 0:
                        callbacks[i].callback(&res, 0, 0); break;
                    case 1:
                        callbacks[i].callback2(&res, 0, 0, req.remote_addr.c_str(), req.remote_port); break;
                    default:
                        ASSERT(false && "wrong callback type");
                    };
                });
                break;
            case REQUEST_POST:
                srv.Post(callbacks[i].endpoint, [=](const httplib::Request& req, httplib::Response& res) {
                    {
                        print("remote addr %s (%d), local addr %s (%d)", req.remote_addr.c_str(), req.remote_port, req.local_addr.c_str(), req.local_port);
                    }
                    if (req.files.size()) {
                        const char* data = (*req.files.begin()).second.content.data();
                        u64 size = (*req.files.begin()).second.content.size();
                        switch (callbacks[i].callback_type) {
                        case 0:
                            callbacks[i].callback(&res, data, size); break;
                        case 1:
                            callbacks[i].callback2(&res, data, size, req.remote_addr.c_str(), req.remote_port); break;
                        default:
                            ASSERT(false && "wrong callback type");
                        };
                    } else {
                        switch (callbacks[i].callback_type) {
                        case 0:
                            callbacks[i].callback(&res, 0, 0); break;
                        case 1:
                            callbacks[i].callback2(&res, 0, 0, req.remote_addr.c_str(), req.remote_port); break;
                        default:
                            ASSERT(false && "wrong callback type");
                        };
                    }
                });
                break;
            }
        }
        srv.set_keep_alive_max_count(INT_MAX);
        srv.set_keep_alive_timeout(INT_MAX);
        bool ls = srv.listen(host, port);
        print("listen res %d", ls);
    });
    srv_thread.detach();
}

void http_set_response(http_response* resp, const char* data, u64 size, const char* content_type) {
    httplib::Response* res = (httplib::Response*)resp;
    res->set_content(data, size, content_type);
}
