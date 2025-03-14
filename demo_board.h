#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include "glad/glad.h"
#include "http.h"
#include <mutex>
#include <thread>
namespace demo_board {
const u32 WG_SIZE = 256; // + copy in the shader
float sizem = 0.055f;
float x_ar = 1;
constexpr u32 POS = 0, ATTR = 1;
u32 max_quads = 1000000;
vertex_buffer vb = {}, vb_aux = {};
bool upd = true;
struct quad { v2 center, hsize; v2 dir, _pad; };
int q_curr = 200000; float speed = 0.3;//int q_curr = 100000; float speed = 1.0;
bool draw = true; draw_data data = {};
u32 compute = 0; u32 q_buf;
// WIP, IDK what is going on by now, something random and unnecessary 💩
char cs_src[1024 * 1024] =  R"(#version 450
#define POS(v) ((v) < 0 ? -(v) : (v))
#define NEG(v) ((v) > 0 ? -(v) : (v))
#define WG_SIZE 256
// todo: no issue being updated dynamically
layout (local_size_x = WG_SIZE) in;
struct quad { vec2 center, hsize; vec4 dir; };
layout (binding = 0, std430) buffer q { quad Q[]; };
layout (binding = 1, std430) writeonly buffer vert_pos { vec4 pos[]; };
layout (binding = 2, std430) /*writeonly*/ buffer vert_attr { vec4 attr[]; };
uniform float fd;
uniform float x_ar;
uniform float sizem;
uniform float speed;
uniform uint curframe;
uniform vec4 mp; // x,y, z - pressed, w - size
uniform vec4 test; // dump, move_mode, dump_enable
void main() {
    uint local_id = gl_LocalInvocationID.x;
    uint group_id = gl_WorkGroupID.x;
    uint i = group_id * gl_WorkGroupSize.x + local_id;

    Q[i].hsize = clamp(abs(Q[i].hsize) * test.x, 0.03, 1.);

    vec2 size = Q[i].hsize * sizem;
    { // update (unoptimized)
        vec2 res = Q[i].center + Q[i].dir.xy * fd;
        if (res.x < size.x) { Q[i].dir.x = POS(Q[i].dir.x); }
        else if (res.x > x_ar - size.x) { Q[i].dir.x = NEG(Q[i].dir.x); }
        else if (res.y < size.y) { Q[i].dir.y = POS(Q[i].dir.y); }
        else if (res.y > 1 - size.y) { Q[i].dir.y = NEG(Q[i].dir.y); }
        Q[i].center = Q[i].center + Q[i].dir.xy * fd * speed;
        if (test.z > 0) // velocity damping enabled
            Q[i].dir = Q[i].dir * test.x; 
        if (mp.z > 0.) { // if mouse is pressed
            vec2 dif = Q[i].center - mp.xy;
            vec2 dir = normalize(dif);
            float v = 0.000001 / dot(dif, dif);
            if (test.y > 0 && length(Q[i].dir.xy) < 1.) { // velocity mode todo: unconditional 
                vec2 addition = dir * v * mp.w;
                if (dot(addition, addition) < 1.0)
                    Q[i].dir.xy += addition.yx*vec2(1,-1);
            }
             else { // velocity mode disabled
                vec2 addition = dir * v * mp.w; 
                Q[i].center += addition;
                if (dot(dif, dif) < (mp.w * 0.0035)) {
                    uint qi = i * 4; // 4 vec4
                    float a = mix(0.9, 0.0, dot(dif,dif) * 100);
                    // asuming we never change the texture coordinates
                    attr[qi + 0] = vec4(1, 1, a, 0); attr[qi + 1] = vec4(1, 0, a, 0);
                    attr[qi + 2] = vec4(0, 0, a, 0); attr[qi + 3] = vec4(0, 1, a, 0);
					//Q[i].hsize = a* vec2(0.3); // uncomment to change the size
                }
            }
        }
    }
    { // draw // todo: 16 bit positions
        uint qi = i * 2; // 4 vec2
        vec2 lb = Q[i].center - size;
        vec2 rt = Q[i].center + size;
        pos[qi + 0] = vec4(rt, rt.x, lb.y); // top right,  bottom right
        pos[qi + 1] = vec4(lb, lb.x, rt.y); // bottom left,  top left 
    }
    {
        uint qi = i * 4; // 4 vec4
        float a = clamp(attr[qi + 0].z + 0.001, 0.1, 1.0);
        // asuming we never change the texture coordinates
        attr[qi + 0] = vec4(1, 1, a, 0); attr[qi + 1] = vec4(1, 0, a, 0);
        attr[qi + 2] = vec4(0, 0, a, 0); attr[qi + 3] = vec4(0, 1, a, 0);
    }
}
)";
char vs_src[1024 * 64] = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aAttr;
out vec4 vAttr;
uniform mat4 rend_m, rend_v, rend_p;
void main() {
    gl_Position = rend_p * vec4(aPos, 0.0, 1.0); // model and view currently unused
    vAttr = aAttr;
})"; 
char fs_src[1024 * 64] = R"(#version 450 core
in vec4 vAttr;
out vec4 FragColor;
uniform sampler2D rend_t0;
uniform sampler2D rend_t1;
void main() {
    vec2 uv = vAttr.xy;
    vec4 color = texture(rend_t0, uv).rgba;
    vec4 color1 = texture(rend_t1, uv).rgba;
    //FragColor = mix(color.gbra, color, vAttr.z);
    FragColor = mix(color, color1, vAttr.z);
    //FragColor.rgba = texture(rend_t0, uv).rgba * vec4(1,1,1,vAttr.z);
    //if (FragColor.a < 0.1) discard; // alpha test
})";

struct record {
    f32 timestamp;
    // todo: more data?
    u8 brash_size; // 0-255 ranging... ..,.
    u8 mouse_on;
    v2 mouse_norm;
};
const u32 MAX_RECORD_COUNT = 240 * 60; 
const u32 SLOT_COUNT = 10;
const u32 SLOT_MSG_COUNT = 128; /* size of a slot_msg string */
struct cool_msg {
    record records[MAX_RECORD_COUNT] = {}; // ~200KB
    u32 records_count = 0;
    char slot_msg[SLOT_MSG_COUNT] = "";
    u32 slot_idx = 0; // used for requests only
    u32 curr_playback = 0; // server only
};

// double buffer slot to copy safely because of reasons
std::mutex b_slots_mt;
cool_msg b_slot;
int b_slot_unprocessed = 0;

// slots to relay
std::mutex r_slots_mt;
cool_msg r_slot[SLOT_COUNT];
int r_slot_unprocessed = 0;

struct st_t {
    enum { magic = 42 };
    cool_msg slots[SLOT_COUNT] = {};
} st;


struct upload_msg_st {
    enum { magic = 43 };
    u32 magic_magic = magic;
    cool_msg slot;
    // slot index/slot_msg is set in slot above
};

f32 lerp(f32 a, f32 b, f32 t) { return a + t * (b - a); }
f32 inlerp(f32 a, f32 b, f32 c) { return (c - a) / (b - a); }

const v2 brash_range = {-5, 5};
float brush_size = 1.0;
i64 rstart = 0;
record create_record(v2 mpos, u8 mon) {
    f32 time = sec(tnow()-rstart);
    u8 brash_u8 = inlerp(brash_range.x, brash_range.y, brush_size) * 255;
    return { time, brash_u8, mon, mpos};
}

#define FOR_SLOT(i) for (int i = 0; i < SLOT_COUNT; i++)

void say_hi(http_response* resp, const char*, u64) {
    http_set_response(resp, "I'm tired", sizeof("I'm tired"), "text/plain");
}

void get_state_callback(http_response* resp, const char*, u64) {
    // todo: double buffer the state  for safety TM
    print("first coord %d", st.slots[0].records_count);
    http_set_response(resp, (c8*)&st, sizeof(st), "application/octet-stream");
}

struct subscribe_data {
    enum {magic = 55};
    int magic_magic = magic;
    int port = 8080;
    char host[64] = {}; // server only
    u64 reserved[8] = {};
};
constexpr int RELAY_MAX = 20;
std::mutex relay_mt;
int relay_curr = 0;
subscribe_data relay_list[RELAY_MAX];

// ??too safe?
int relay_curr_render = 0;
subscribe_data relay_list_render[RELAY_MAX];

void post_subscribe_callback2(http_response* resp, const char* post_data, u64 post_data_size, const char* host, int port) {
    const char* text_response = "Post OK";
    // add to a relay list
    print("host %s (p? %d) subscibing\n", host, port);
    if (post_data && post_data_size == sizeof(subscribe_data)) {
        subscribe_data* sd = (subscribe_data*)post_data;
        // todo: validate all the input
        bool invalid_data = !(sd->magic_magic == subscribe_data::magic);
        if (!invalid_data) {
            relay_mt.lock(); defer{ relay_mt.unlock(); };
            // find in the list -> if no, add
            bool found = false;
            for (int i = 0; i < relay_curr; i++) {
                subscribe_data &ref_sd = relay_list[i];
                if (strcmp(host, ref_sd.host) == 0) {
                    // host exists, update stuff maybe
                    ref_sd = *sd;
                    memcpy(ref_sd.host, host, strlen(host)); // overvite empty host, since there was a copy above. todo: check if oobs
                    found = true;
                    break;
                }
            }
            if (!found) {
                subscribe_data &ref_sd = relay_list[relay_curr];
                ref_sd = *sd;
                memcpy(ref_sd.host, host, strlen(host));
                relay_curr = (relay_curr + 1) % RELAY_MAX;
            }
            
        } else text_response = "Post Fail: invalid magic";
    }
    else {
        text_response = "Post Fail: invalid size or data";
    }
    http_set_response(resp, text_response, strlen(text_response), "text/plain");
}

void post_state_callback(http_response* resp, const char* post_data, u64 post_data_size) {
    const char* text_response = "Post OK"; // todo: binary error code
    if (post_data && post_data_size == sizeof(upload_msg_st)) {
        upload_msg_st* slot_from_client = (upload_msg_st*)post_data;
        // todo: validate all the input
        bool invalid_data = !(slot_from_client->magic_magic == upload_msg_st::magic);
        if (!invalid_data) {
            b_slots_mt.lock(); defer{ b_slots_mt.unlock(); };
            b_slot = slot_from_client->slot;
            b_slot_unprocessed = 1;
            // todo: multiple buffered commands
            /*if (unprocessed_commands < MAX_COMMANDS) { // todo: handle it differently
                commands[unprocessed_commands] = *com;
                unprocessed_commands++;
            }
            else text_response = "Post Fail: maximum server commands";
            */
        } else text_response = "Post Fail: invalid magic";
    }
    else {
        text_response = "Post Fail: invalid size or data";
    }
    http_set_response(resp, text_response, strlen(text_response), "text/plain");
}
static char host[64] = "0.0.0.0";
bool stop_bc_thread = false; // not set to true, so who cares..?
bool pause_bc_thread = false;
static float broadcast_sleep_sec = 1.0f;
void start_broadcast_thread() {
    std::thread th([]() { // todo: avoid circular dependencies if someone you subscibed to yourself or there is a ciclical dep
        static cool_msg l_slot[SLOT_COUNT] = {};
        static subscribe_data l_relay_list[RELAY_MAX] = {};
        int relay_todo = 0;
        int slots_todo = 0;
        while (!stop_bc_thread) {
            if (!pause_bc_thread) {
                {
                    r_slots_mt.lock(); defer{ r_slots_mt.unlock(); };
                    slots_todo = r_slot_unprocessed;
                    if (slots_todo) {
                        memcpy(l_slot, r_slot, sizeof(cool_msg) * slots_todo);
                    }
                    r_slot_unprocessed = 0;
                }

                {
                    relay_mt.lock(); defer{ relay_mt.unlock(); };
                    relay_todo = relay_curr;
                    if (relay_todo) {
                        memcpy(l_relay_list, relay_list, sizeof(subscribe_data) * relay_todo);
                    }
                    // don't clear relay_list here since we need to keep it
                }
                // for each host
                while (relay_todo > 0) {
                    int relay_id = relay_todo - 1;
                    const char* host = l_relay_list[relay_id].host;
                    int port = l_relay_list[relay_id].port;

                    while (slots_todo > 0) {
                        int id = slots_todo - 1;
                        upload_msg_st up = {};
                        up.magic_magic = upload_msg_st::magic;
                        up.slot = l_slot[id];
                        up.slot.slot_msg[0] = '!';

                        static char post_data_buf[128] = {0};
                        static buffer_ex post_data = { post_data_buf, 0, 128 };
                        if (http_post(host, port, "/state", { (u8*)&up, sizeof(up) }, &post_data) == HTTP_ERROR) {
                            print("error doing post request");
                        } else {
                            post_data_buf[127] = 0;
                            print("Post request: %s", post_data.data);
                        }
                        slots_todo--;
                    }
                    relay_todo--;
                }
            }
            tsleep(nano(broadcast_sleep_sec)); // IDK
        }
    });
    th.detach();
}

buffer_ex get_data = {};

constexpr int MY_PORT = 8080; // todo: configurable

void init(rend& R) {

    {
        buffer host_file = read_file("relay_host.txt", 64);
        if (host_file.size > 0 && host_file.size < 64)
            memcpy(host, host_file.data, 64);
    }

    static server_callback cbk[] = {
        {REQUEST_GET, "/hi", say_hi},
        {REQUEST_POST, "/subscribe", nullptr, post_subscribe_callback2, 1},
        {REQUEST_GET, "/state", get_state_callback},
        {REQUEST_POST, "/state", post_state_callback},
    };

    if (!compute) {
        start_server("0.0.0.0", MY_PORT, ARSIZE(cbk), cbk);

        start_broadcast_thread();

        print("data size %d", sizeof(cool_msg));

        get_data = { (c8*)alloc(sizeof(st)), 0, sizeof(st) }; // todo: could just be a pointer to st ...

        compute = R.shader(0, 0, cs_src);
        R.textures[R.curr_tex++] = data.tex[0] = R.texture("amogus.png");//R.texture("pepe.png");
        R.textures[R.curr_tex++] = data.tex[1] = R.texture("pepe.png");//R.texture("pepe.png");
        R.progs[R.curr_progs++] = data.prog = R.shader(vs_src, fs_src);
        float x_ar = R.wh.x / (float)R.wh.y;
        i64 istart = tnow();
        q_buf = R.ssbo(sizeof(quad) * max_quads, 0, true, 0 /*init with 0s*/);
        quad* q = (quad*)map(q_buf, 0, sizeof(quad) * max_quads, MAP_WRITE); defer{ unmap(q_buf); };
        vb = { {}, max_quads * 4, 2, {
            {0, AT_FLOAT, 2},
            {0, AT_FLOAT, 4}
        } };
        init_vertex_buffer(&vb);
        init_quad_indicies(vb.ib.index_buf, max_quads);

        v4* attr = (v4*)map(vb.ab[ATTR].id, 0, sizeof(v4) * max_quads * 4, MAP_WRITE);
        u32 curr_quad_count = 0;
        for (u32 i = 0; i < max_quads; i++) { // units coord system
            float size = 0.2f * RN();
            v2 hsize = v2{ size,size } / 2.f;
            q[i].hsize = hsize;
            q[i].center = { RN() * x_ar, RNC(hsize) };
            q[i].dir = { RNC(-1.f,1.f) * 0.05f, RNC(-1.f, 1.f) * 0.07f};
            v4 base_attr[4] = {{1, 1, 0.3f, 0},
                                {1, 0, 0.3f, 0},
                                {0, 0, 0.3f, 0},
                                {0, 1, 0.3f, 0} };
            memcpy((u8*)attr + curr_quad_count * sizeof(v4) * 4, base_attr, sizeof(v4) * 4);
            curr_quad_count++;
        }
        unmap(vb.ab[ATTR].id);
        data.ib = vb.ib;

        FOR_SLOT(i) {
            memset(st.slots[i].slot_msg, 0, SLOT_MSG_COUNT);
            st.slots[i].slot_msg[0] = '0' + i; st.slots[i].slot_msg[1] = ' ';
            st.slots[i].slot_idx = i;
            memcpy(st.slots[i].slot_msg + 2, "test name", sizeof("test name"));
        }

        print("init time :%f", sec(tnow()-istart));
    }
}
void update(rend& R) {
    static bool mon = false;
    static float damp = 0.999f;
    static bool damp_enabled = false;
    static bool velocity_mode = false;
    if (R.key_pressed('F', KT::IGNORE_IMGUI))
        mon = !mon;
    
    x_ar = R.wh.x / (float)R.wh.y;
    data.p = ortho(0, x_ar, 0, 1);

    // todo: clear resources
    if (ImGui::TreeNode("Shader VSFS")) {
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
		ImGui::InputTextMultiline("##source1", fs_src, IM_ARRAYSIZE(fs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), flags);
        ImGui::InputTextMultiline("##source2", vs_src, IM_ARRAYSIZE(vs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), flags);
		if (ImGui::Button("Compiler go brr")) {
			data.prog = R.shader(vs_src, fs_src);
		}
		ImGui::TreePop();
	}

    if (ImGui::TreeNode("Shader Compute")) {
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
		ImGui::InputTextMultiline("##source3", cs_src, IM_ARRAYSIZE(cs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 32), flags);
		if (ImGui::Button("Compiler go brr")) {
			compute = R.shader(0, 0, cs_src);
		}
		ImGui::TreePop();
	}

    static int curr_slot = 0;
    static int port = 8080;
    {
        ImGui::Separator();
        ImGui::SliderInt("upload slot IDX", &curr_slot, 0, SLOT_COUNT);
        char* desc = st.slots[curr_slot].slot_msg;
        ImGui::InputText("short description", desc, SLOT_MSG_COUNT);
        if (ImGui::Button("Upload Your Cool And Not Cringe Message (I know your IP)")) {
            upload_msg_st up = {};
            up.magic_magic = upload_msg_st::magic;
            up.slot = st.slots[curr_slot];

            static char post_data_buf[128] = {0};
            static buffer_ex post_data = { post_data_buf, 0, 128 };
            if (http_post(host, port, "/state", { (u8*)&up, sizeof(up) }, &post_data) == HTTP_ERROR) {
                print("error doing post request");
            } else {
                post_data_buf[127] = 0;
                print("Post request: %s", post_data.data);
            }
        }


        if (ImGui::Button("Sync Records With Host")) {
            if (http_get(host, port, "/state", &get_data) != HTTP_ERROR) {
                //cs_mt.lock(); defer{ cs_mt.unlock(); }; // do I really need it?
                memcpy(&st, get_data.data, sizeof(st));
            }
        }
        ImGui::Separator();

        FOR_SLOT(i) {
            ImGui::PushID(i);
            // todo: calculate msg duration and add it as a text info
            if (ImGui::RadioButton(st.slots[i].slot_msg, &curr_slot, i)) {
                // change to slot (curr_slot = i implicitly)
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::InputInt("Port", &port);
        ImGui::InputText("Host", host, sizeof(host));
        ImGui::Separator();
        ImGui::Text("relay list count: %d", relay_curr); // without lock, yolo
        if (relay_curr > 0) { // without lock, yolo
            {
                relay_mt.lock(); defer{ relay_mt.unlock(); };
                relay_curr_render = relay_curr;
                memcpy(relay_list_render, relay_list, sizeof(relay_list));
            }
            if (ImGui::Button("Reset relay")) {
                relay_mt.lock(); defer{ relay_mt.unlock(); };
                relay_curr = 0;
                memset(relay_list, 0, sizeof(relay_list));
            }
            for (int i = 0; i < relay_curr_render; i++) {
                if (ImGui::Button("Clear")) {
                    // swap with the last
                    relay_mt.lock(); defer{ relay_mt.unlock(); };
                    if (i != relay_curr - 1) {
                        relay_list[i] = relay_list[relay_curr - 1];
                    }
                    memset(relay_list + relay_curr - 1, 0, sizeof(subscribe_data)); //?
                    relay_curr--; // assert >= 0?
                }
                ImGui::SameLine();
                ImGui::Text("* %s : %d", relay_list_render[i].host, relay_list_render[i].port);
            }
            ImGui::Checkbox("Broadcast Paused", &pause_bc_thread);
        }
        if (ImGui::Button("Subscribe To Relay For Updates")) {
            subscribe_data sb = {};
            sb.port = MY_PORT;
            static char post_data_buf[128] = {0};
            static buffer_ex post_data = { post_data_buf, 0, 128 };
            if (http_post(host, port, "/subscribe", { (u8*)&sb, sizeof(sb) }, &post_data) == HTTP_ERROR) {
                print("error doing post request");
            } else {
                post_data_buf[127] = 0;
                print("Post request: %s", post_data.data);
            }
        }
        ImGui::Separator();
    }

    if (ImGui::TreeNode("Quad Params")) {
        ImGui::SliderInt("quads", &q_curr, 0, max_quads - WG_SIZE);
        q_curr = ALIGN_UP(q_curr, WG_SIZE);
        ImGui::SliderFloat("speed", &speed, 0.f, 3.f);
        ImGui::SliderFloat("size", &sizem, 0.001f, 0.5f);
        ImGui::Checkbox("draw", &draw);
        ImGui::Checkbox("update", &upd);
        ImGui::Checkbox("mouse always on [F]", &mon);
        ImGui::Checkbox("speed damp enabled", &damp_enabled);
        ImGui::Checkbox("velocity mode enabled", &velocity_mode);
        ImGui::SliderFloat("dampening", &damp, 0.995f, 1, "%.6f");
        ImGui::SliderFloat("brush size", &brush_size, brash_range.x, brash_range.y);
        ImGui::TreePop();
    }

    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    if (upd) {

        if (b_slot_unprocessed > 0) {
            bool success = false;
            u32 slot_id = 0;
            print("processing command, left %d", b_slot_unprocessed);
            {
                b_slots_mt.lock(); defer{ b_slots_mt.unlock(); };
                // copy to the correct slot
                slot_id = b_slot.slot_idx;
                print("slot_id from cliend %d", slot_id);
                b_slot_unprocessed--;

                if (slot_id >= 0 && slot_id <= SLOT_COUNT - 1) {
                    memcpy(st.slots + slot_id, &b_slot, sizeof(cool_msg));
                    success = true;
                }
            }
            print("processing %d", success);

            // broadcast to relay list in a different thread
            if (success) {
                r_slots_mt.lock(); defer{ r_slots_mt.unlock(); };
                std::mutex r_slots_mt;
                memcpy(r_slot + r_slot_unprocessed, st.slots + slot_id, sizeof(cool_msg));
                r_slot_unprocessed = (r_slot_unprocessed + 1) % SLOT_COUNT;
            }
        }

        v2 mouse_norm = R.mouse_norm();
        bool mouse_on = mon || R.key_pressed(KT::MBL) || R.key_pressed(KT::MBR);

        u32 *i = &st.slots[curr_slot].records_count;
        bool r_now = R.key_pressed('R');
        static bool r_prev = false; defer { r_prev = r_now; };
        static bool r_on = false;
        if (r_now && !r_prev) {
            r_on = !r_on;
            if (r_on) { // record start
                rstart = tnow();
                st.slots[curr_slot].records_count = 0;
            } else { // record end
                st.slots[curr_slot].records_count = *i;
            }
        }

        ImGui::Text("r_now %d r_on %d", r_now, r_on);

        if (r_on) {
            record* r = st.slots[curr_slot].records;
            r[*i] = create_record(mouse_norm, mouse_on);
            (*i)++;
            if (*i >= MAX_RECORD_COUNT) {
                r_on = false; // end record
            }
        }

        u32* c = &st.slots[curr_slot].curr_playback;
        u32 record_count = st.slots[curr_slot].records_count;
        record* r = st.slots[curr_slot].records;

        bool p_now = R.key_pressed('P');
        static bool p_prev = false; defer { p_prev = p_now; };
        static bool p_on = false;
        if (p_now && !p_prev) {
            p_on = !p_on;
            // disable recording if on
            r_on = false;
            *c = 0;
        }

        if (p_on) {
            mouse_norm = r[*c].mouse_norm;
            mouse_on = r[*c].mouse_on;
            (*c)++;
            if (*c >= record_count || *c >= MAX_RECORD_COUNT) {
                *c = 0;
                curr_slot = (curr_slot + 1) % SLOT_COUNT;
            }
            // todo: do some interpolation with timestamp
        }

        ImGui::Text("p_now %d p_on %d", p_now, p_on);

        ImGui::Text("mnorm %f %f", mouse_norm.x, mouse_norm.y);

        int group_size = q_curr / WG_SIZE;
        u32 pos_buf = vb.ab[POS].id;
        u32 attr_buf = vb.ab[ATTR].id; // errors? not used currently
        v4 mp = {mouse_norm.x * x_ar, mouse_norm.y, mouse_on ? 1.f : 0.f, R.key_pressed(KT::MBR) ? -brush_size : brush_size };
        v4 test = {damp, (f32)velocity_mode, (f32)damp_enabled};
        static u32 curframe = 0;
        curframe++;
        R.dispatch({ compute, (u32)group_size, 1, 1, {q_buf, pos_buf, attr_buf}, {
            {"fd",    UNIFORM_FLOAT, &R.fd},
            {"x_ar",  UNIFORM_FLOAT, &x_ar},
            {"sizem", UNIFORM_FLOAT, &sizem},
            {"speed", UNIFORM_FLOAT, &speed},
            {"mp", UNIFORM_VEC4, &mp},
            {"test", UNIFORM_VEC4, &test},
            {"curframe", UNIFORM_UINT, &curframe}
        }});
        R.ssbo_barrier(); // wait for the compute shader to finish - todo: double buffer?
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    i64 qf_start = tnow();
    if (draw) {
        data.ib = vb.ib;
        data.ib.vertex_count = q_curr * 6;
        R.submit(&data, 1);
    }
}
}
