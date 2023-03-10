#include "rend.h"
#include "imgui/imgui.h"
#include "http.h"
#include "demo_rts.h"

namespace client {

buffer data;
char host[32] = "localhost";//"10.40.14.40";
int port = 5555;

void init(rend &R) {

}

void update(rend &R) {
    using namespace ImGui;
    Begin("RTS");
    if (R.wh.x != R.wh.y && Button("Scale")) {
        R.win_size({R.wh.x, R.wh.x});
    }

    if (Button("ask hi")) {
        u64 ts = tnow();
        data = http_get(host, port, "/hi");
        float ss = sec(tnow()-ts);
        print("here %s, time: %f", data.data, ss);
    }

     if (Button("ask hi bin")) {
        u64 ts = tnow();
        data = http_get(host, port, "/hibin");
        float ss = sec(tnow()-ts);
        v2 d = *(v2*)data.data;
        
        //memcpy (&d, data.data, sizeof(v2));
        for (int i = 0; i < data.size; i++) {
            print("%d, 0x%X", i, data.data[i]);
        }
        print("here %f, time: %f", d.x, ss);
    }

    struct bin {v2 pos;};
    static bin file_data = {10.f, 20.f};
    SliderFloat2("Pos", (float*)&file_data, 0, 100);
    if (Button("post")) {
        u64 ts = tnow();
        buffer file = {(u8*)&file_data, sizeof(file_data)};
        data = http_post(host, port, "/post_test", file);
        float ss = sec(tnow()-ts);
        print("here %s, time: %f", data.data, ss);
    }


    Text("Response: %s", data.data);

    R.clear({1, 0, 0, 0});

    End();
}

}