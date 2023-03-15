#include "rend.h"
#include "imgui/imgui.h"
#include "http.h"
#include "demo_rts.h"

namespace client {
using namespace rts;

draw_data dd;
char host[32] = "10.40.14.40";
int port = 8080;
const u64 DATA_SIZE = 1024 * 1024 * 1024;
buffer_ex data;

void init(rend &R) {
    if (!data.data) {
        data = { (c8*)alloc(DATA_SIZE), 0, DATA_SIZE };
        dd.p = ortho(0, ARENA_SIZE, 0, ARENA_SIZE);
        //const char* textures[] = { "star.png", "cloud.png", "heart.png", "lightning.png", "res.png" };
        const char* textures[] = { "amogus.png", "din.jpg", "pool.png", "pepe.png", "coffee.png" };
        for (int i = 0; i < ARSIZE(textures); i++) R.textures[R.curr_tex++] = dd.tex[i] = R.texture(textures[i]);
        R.progs[R.curr_progs++] = dd.prog = R.shader(R.vs_quad, R"(#version 450 core
            in vec4 vAttr;
            out vec4 FragColor;
            uniform sampler2D rend_t0;
            uniform sampler2D rend_t1;
            uniform sampler2D rend_t2;
            uniform sampler2D rend_t3;
            uniform sampler2D rend_t4;
            void main() {
                vec2 uv = vAttr.xy;
                if (vAttr.w == 0) FragColor.rgba = texture(rend_t0, uv);
                else if (vAttr.w == 1) FragColor.rgba = texture(rend_t1, uv);
                else if (vAttr.w == 2) FragColor.rgba = texture(rend_t2, uv);
                else if (vAttr.w == 3) FragColor.rgba = texture(rend_t3, uv);
                else if (vAttr.w == 4) FragColor.rgba = texture(rend_t4, uv);
                else if (vAttr.w == 5) {
                    FragColor.rgba = vec4(vAttr.xyz, 0.25 );
                }
                else
                    FragColor.rgba = vec4(1, 0, 0, 1); // debug
            })");
    }
}

void post_command(update_command com) {
    if (http_post(host, port, "/state", { (u8*)&com, sizeof(com) }, &data) == HTTP_ERROR) {
        print("error doing post request");
    } else {
        if (memcmp(data.data, "Post Ok", sizeof("Post Ok")) == 0) {
            print("Post good");
        } else {
            print("Post bad");
        }
    }
}

void update(rend &R) {
    using namespace ImGui;
    Begin("RTS"); defer{ End(); };

    if (R.wh.x != R.wh.y && Button("Scale")) R.win_size({R.wh.x, R.wh.x});
    InputText("host", host, sizeof(host));
    InputInt("port", &port);

    if (Button("ask hi")) {
        u64 ts = tnow();
        http_get(host, port, "/hi", &data);
        print("here %s, time: %f", data.data, sec(tnow() - ts));
    }

    static v2 go_pos = {10.f, 20.f};
    SliderFloat2("Pos", (f32*)&go_pos, 0, 100);
    if (Button("post")) {
        u64 ts = tnow();
        if (http_post(host, port, "/post_test", { (u8*)&go_pos, sizeof(go_pos) }, &data) == HTTP_ERROR) {
            print("error doing post");
        }
        if (!data.data) {
            print("empty post data");
        }
        print("here %s, time: %f", data.data, sec(tnow() - ts));
    }

    static bool always_on = false; Checkbox("Always On", &always_on);
    static rts::current_state cs = {};
    static f32 time = 0;
    static char strup[128] = { 0 };
    memcpy(strup, data.data, MIN(128, data.size));
    Text("Update Response: %s", data.data);

    if (always_on || Button("Get State")) {
        u64 ts = tnow(); defer{ time = sec(tnow() - ts); };
        if (http_get(host, port, "/state", &data) == HTTP_ERROR) {
            print("GET STATE ERROR");
        } else if (!data.data) {
            print("empty_data");
        } else {
            print("work, size: %d", data.size);
            memcpy(&cs, data.data, sizeof(cs));
        }
    }
    Text("Server frame: %llu", cs.frame_count);
    Text("Unit 11 pos: %f %f, time: %f", cs.info[11].pos.x, cs.info[11].pos.y, time);
    static char str[128] = { 0 };
    memcpy(str, data.data, MIN(128, data.size));
    Text("State Response: %s", data.data);

    v2 mov_dir = {};
    static v2 prev_mov_dir = {}; defer{ prev_mov_dir = mov_dir; };
    if (R.key_pressed('W')) mov_dir.y += 1.f;
    if (R.key_pressed('S')) mov_dir.y -= 1.f;
    if (R.key_pressed('A')) mov_dir.x -= 1.f;
    if (R.key_pressed('D')) mov_dir.x += 1.f;
    bool update_go = false;
    if (mov_dir != prev_mov_dir) {
        update_go = true; // todo: if new direction
        if (mov_dir) mov_dir = norm(mov_dir); // fix diagonal, don't divide by 0
        go_pos = clamp(cs.info[11].pos + mov_dir * ARENA_SIZE, { 0,0 }, { ARENA_SIZE, ARENA_SIZE }); // hm
    }
    if (R.key_pressed(KT::MBL)) {
        go_pos = R.mouse_norm() * ARENA_SIZE;
        update_go = true;
    }
    if (update_go || ImGui::Button("Test Go")) {
        update_command com = {};
        com.team_id = 1;
        com.update_mask[1] = true;
        com.action[1] = ACTION_GO;
        com.go_target[1] = go_pos;
        post_command(com);
    }
    if (ImGui::Button("Test Grab") || R.key_pressed(' ')) {
        update_command com = {};
        com.team_id = 1;
        com.update_mask[1] = true;
        com.action[1] = ACTION_GRAB;
        // find closest COFF
        u32 id = com.team_id * MAX_UNIT + 1;
        u32 target_id = 0;
        f32 min_len = ARENA_SIZE;
        FOR_COFF(i) {
            f32 l = len(cs.info[i].pos - cs.info[id].pos);
            if (l < min_len && l < EPS_GRAB) {
                target_id = i;
                min_len = l;
            }
        }
        if (target_id) {
            com.obj_id_target[1] = target_id;
            post_command(com);
        }
    }

    #define SHADER_QUAD 5.0f
    #define SHADER_COFF 4.0f
    v4 spawn_color[MAX_TEAMS] = {
        // w reserved for shader id
        {1, 0, 0, SHADER_QUAD},
        {0, 1, 0, SHADER_QUAD},
        {1, 0, 1, SHADER_QUAD},
        {1, 0, 1, SHADER_QUAD},
    };
    for (int i = 0; i < cs.info_size; i++) { // todo: move to demo_rts.h
        object_state& obj = cs.info[i];
        switch (obj.type) {
        case OBJ_UNIT:
            R.quad_t(obj.pos - UNIT_SIZE / 2.f, obj.pos + UNIT_SIZE / 2.f, { 0.f, (f32)obj.team_id });
            break;
        case OBJ_COFF:
            // todo: if grabbed - draw with transparent alpha
            R.quad_t(obj.pos - MAX_COFF_SIZE / 2.f, obj.pos + MAX_COFF_SIZE / 2.f, { 0.f, SHADER_COFF });
            break;
        case OBJ_SPAWN:
            R.quad(obj.pos - SPAWN_SIZE / 2.f, obj.pos + SPAWN_SIZE / 2.f, spawn_color[obj.team_id]);
            break;
        default: break;
        }
    }

    static bool cursor = false;
    Checkbox("Cursor", &cursor);
    if (cursor) {
        v2 pos = R.mouse_norm() * ARENA_SIZE;
        v2 size = { 2.f, 2.f };
        R.quad(pos - size / 2.f, pos + size / 2.f, {0, 1, 0, SHADER_QUAD});
    }

    R.clear({0.2f, 0.f, 0.f, 0.f});
    R.submit(dd);
}

}