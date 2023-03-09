#include "rend.h"
#include "imgui/imgui.h"
#include "http.h"

/*
server endpoints:
GET state?id=N - todo: pin=PIN
POST state?id=N - todo: pin=PIN

state binary data: look at state struct

todo: join
todo: leave?id=N&pin=PIN
*/

namespace client {

// WIP
#define MAX_UNIT 10
#define MAX_TEAMS 4
#define MAX_COFF 10
#define MAX_INFO ((MAX_UNIT * MAX_TEAMS) + MAX_TEAMS + MAX_COFF)
#define ID_COFF 0xFF
enum obj_type {
    UNIT, // both self and enemy
    COFF,
    SPAWN // both self and enemy
};
enum obj_state_type {
    NONE,
    COFF_IDLE, // todo: COFF_IDLE_SPAWN?
    COFF_TAKEN,
    UNIT_IDLE,
    UNIT_GRAB,
    UNIT_TAKEN, // :)
};
struct object_state {
    v2 pos = {};
    i32 energy = 0;
    obj_type type;
    obj_state_type st;
    u32 obj_id; // from 0 to MAX_INFO
    u8 team_id; // ID_COFF for coffee
};
enum event_type {
    GRAB_SUCCESS,
    GRAB_FAIL,
};
struct unit_event {
    u32 obj_id;
};
struct current_state {
    int magic = 0xDEADBEEF;
    int version = 1;
    u64 score[MAX_TEAMS] = {};
    i32 info_size = 0;  // number of visible objects in the radius // todo: specify radius
    object_state info[MAX_INFO] = {}; // todo: make info per unit? so its visible who sees it?
    unit_event last_events[MAX_UNIT];
};
struct update_unit_state {
    u8 update_mask[MAX_UNIT] = {};
    v2 new_direction[MAX_UNIT] = {};
};

buffer data;
char host[32] = "10.40.14.40";
int port = 8080;

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
        printf("here %s, time: %f\n", data.data, ss);
    }

     if (Button("ask hi bin")) {
        u64 ts = tnow();
        data = http_get(host, port, "/hibin");
        float ss = sec(tnow()-ts);
        v2 d = *(v2*)data.data;
        
        //memcpy (&d, data.data, sizeof(v2));
        for (int i = 0; i < data.size; i++) {
            printf("%d, 0x%X\n", i, data.data[i]);
        }
        printf("here %f, time: %f\n", d.x, ss);
    }

    struct bin {v2 pos;};
    static bin file_data = {10.f, 20.f};
    SliderFloat2("Pos", (float*)&file_data, 0, 100);
    if (Button("post")) {
        u64 ts = tnow();
        buffer file = {(u8*)&file_data, sizeof(file_data)};
        data = http_post(host, port, "/post_test", file);
        float ss = sec(tnow()-ts);
        printf("here %s, time: %f\n", data.data, ss);
    }


    Text("Response: %s", data.data);

    R.clear({1, 0, 0, 0});

    End();
}

}