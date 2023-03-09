#include "std.h"
#include "http.h"
#include "httplib.h"
#include <stdlib.h> // memcpy

buffer http_get(const char* host, int port, const char* req) {
    buffer buf = {0,0};
    httplib::Client cli(host, port);
    //cli.set_connection_timeout(0, 100000); // 300 milliseconds
    auto res = cli.Get(req);
    if (res) {
        printf("resbody: %s\n", res->body.c_str());
        buf.data = alloc(res->body.size() + 1/*?*/);
        buf.size = res->body.size();
        buf.data[buf.size] = 0; // to be safe
        memcpy(buf.data, res->body.data(), res->body.size());
    } else {
        printf("Error code : %d\n", (int)res.error());
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
        printf("resbody: %s\n", res->body.c_str());
        buf.data = alloc(res->body.size() + 1/*?*/);
        memcpy(buf.data, res->body.data(), res->body.size() + 1);
    } else {
        printf("Error code : %d\n", (int)res.error());
    }
    return buf;
}