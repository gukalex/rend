#include "std.h"
#include "http.h"
#include "httplib.h"
#include <stdlib.h> // memcpy
#include <thread>

buffer http_get(const char* host, int port, const char* req) {
    buffer buf = {0,0};
    httplib::Client cli(host, port);
    //cli.set_connection_timeout(0, 100000); // 300 milliseconds
    auto res = cli.Get(req);
    if (res) {
        print("resbody: %s", res->body.c_str());
        buf.data = alloc(res->body.size() + 1/*?*/);
        buf.size = res->body.size();
        buf.data[buf.size] = 0; // to be safe
        memcpy(buf.data, res->body.data(), res->body.size());
    } else {
        print("Error code : %d", (int)res.error());
    }
    return buf;
}

buffer http_post(const char* host, int port, const char* req, buffer file) {
    buffer buf = {0,0};
    httplib::Client cli(host, port);
    httplib::MultipartFormDataItems items;
    if (file.data) {
        std::string data;
        data.resize(file.size);
        memcpy(&data[0], file.data, file.size);
        items.push_back({"image_file", data, "image_file", "application/octet-stream"}); /*todo:file name*/
    }
    auto res = cli.Post(req, items);
    if (res) {
        print("resbody: %s\n", res->body.c_str());
        buf.data = alloc(res->body.size() + 1/*?*/);
        memcpy(buf.data, res->body.data(), res->body.size() + 1);
    } else {
        print("Error code : %d\n", (int)res.error());
    }
    return buf;
}

void start_server(const char* host, int port, int count, server_callback *callbacks) {
    std::thread srv_thread([=]() {
        httplib::Server srv;
        for (int i = 0; i < count; i++) {
            switch (callbacks[i].type) {
            case req_type::GET:
                srv.Get(callbacks[i].endpoint, [=](const httplib::Request&, httplib::Response& res) {
                    char* data = (c8*)alloc(1024 * 1024 * 1024);
                    u64 size = 0;
                    const char* content_type = nullptr;
                    callbacks[i].callback(data, &size, &content_type);
                    res.set_content(data, size, content_type);
                });
                break;
            case req_type::POST:
                break;
            }
        }
        srv.set_keep_alive_max_count(INT_MAX);
        srv.set_keep_alive_timeout(INT_MAX);
        srv.listen(host, port);
    });
    srv_thread.detach();
}