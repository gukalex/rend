/*
CONFIGURE:
(./conf.sh)
download imgui, stb_image, httplib (glfw should be installed manually)

BUILD:
(./build.sh)
build object files for libraries we are not gonna touch (slow):
g++ -c glad.c stb_image.c -I. -Iimgui/backends -Iimgui imgui/imgui_unity.cpp -O3
build our demos (consider moving std.cpp/rend.cpp in the line above):
g++ client.cpp rend.cpp std.cpp -I. glad.o stb_image.o -Iimgui/backends -Iimgui imgui_unity.o -ldl -lglfw -lGLU -lGL -lXrandr -lX11 -lrt -O3 -o client
*/

#include "rend.h"
#include "imgui/imgui.h"
#include "http.h"
#include "demo_rts.h"
#include "rts_render.h"

#include <stdio.h>
#include <thread>
#include <mutex>

using namespace rts;

char host[32] = "10.40.14.40";
//char host[32] = "127.0.0.1";
int port = 8080;
const u64 DATA_SIZE = 1024 * 1024 * 1024;
buffer_ex post_data;

tazer_ef tz_ef[MAX_TAZER_EF] = {};

// debug state
constexpr int MAX_DEBUG_FRAMES = 2000;// 1 state is around 3KB, so 2000 is only several sMBs
current_state* cst;

std::mutex cs_mt;
current_state cur_st = {};

bool stop_update_thread = false;  // todo: threadsafe
bool pause_update_thread = false; //
avg_data avg_server_update = avg_init(0.5f); //
void start_update_thread() {
    std::thread th([]() {
        buffer_ex get_data = { (c8*)alloc(sizeof(cur_st)), 0, sizeof(cur_st) }; defer{ dealloc(get_data.data); };
        ASSERT((sizeof(tazer_ef) * MAX_TAZER_EF) < sizeof(cur_st));
        u64 prev_frame_count = 0;
        while (!stop_update_thread) {
            if (!pause_update_thread) {
                i64 ts = tnow();
                if (http_get(host, port, "/state", &get_data) != HTTP_ERROR) {
                    cs_mt.lock(); defer{ cs_mt.unlock(); }; // do I really need it?
                    memcpy(&cur_st, get_data.data, sizeof(cur_st));
                }
                if (http_get(host, port, "/tazers", &get_data) != HTTP_ERROR) { // todo: on a different thread?
                    memcpy(tz_ef, get_data.data, sizeof(tazer_ef) * MAX_TAZER_EF);
                }
                avg(sec(tnow() - ts), &avg_server_update);
                //if (cur_st.frame_count == prev_frame_count) print("redundant update %d", cur_st.frame_count);
                //else print("non redundant update %d", cur_st.frame_count);
                if (cur_st.frame_count - prev_frame_count > 1) print("spike from %d to %d", prev_frame_count, cur_st.frame_count);
                prev_frame_count = cur_st.frame_count;
            }
            tsleep(nano(1/60.f * 0.2f)); // IDK
        }
    });
    th.detach();
}

void post_command(update_command com) {
    if (http_post(host, port, "/state", { (u8*)&com, sizeof(com) }, &post_data) == HTTP_ERROR) {
        print("error doing post request");
    } else {
        if (memcmp(post_data.data, "Post OK", sizeof("Post OK")) != 0) {
            print("Bad post request: %s", post_data.data);
        }
    }
}

void client_update(rend& R, current_state& cs) {
    using namespace ImGui;

    if (R.wh.x != R.wh.y && Button("Scale")) R.win_size({ R.wh.x, R.wh.x });
    InputText("host", host, sizeof(host), ImGuiInputTextFlags_EnterReturnsTrue);
    InputInt("port", &port, ImGuiInputTextFlags_EnterReturnsTrue);
    if (Button("ask hi")) http_get(host, port, "/hi", &post_data);
    Checkbox("Pause", &pause_update_thread);

    {
        static int ccst = 0; // current inspected index
        static int offset = 0;
        static bool recording = true;
        static bool first_fill = true;
        int curr_debug = ccst;
        static int last_written = 0;
        int min_offset = first_fill ? MAX(-MAX_DEBUG_FRAMES + 1, -ccst + 1) : -MAX_DEBUG_FRAMES + 1;
        if (ImGui::Button(recording ? "Stop Recording" : "Continue Recording")) { recording = !recording; }
        else { Text("Recording frame %d out of %d", ccst, MAX_DEBUG_FRAMES); }
        if (recording && cst[last_written].frame_count != cs.frame_count) { // record only new frames
            last_written = ccst;
            cst[ccst++] = cs;
            if (ccst == MAX_DEBUG_FRAMES) {
                first_fill = false;
                ccst = 0;
            }
            if (offset == -MAX_DEBUG_FRAMES + 1) offset = 0;
            if (offset != 0) offset--;
        }

        SameLine(); if (R.key_pressed('<') || Button("<")) { offset--; };
        SameLine(); if (R.key_pressed('>') || Button(">")) { offset++; }
        if (SliderInt("Offset", &offset, min_offset, 0)) recording = offset == 0;// false;
        int debug_state = ((curr_debug + offset) < 0 ? (-min_offset + (curr_debug + offset)) : (curr_debug + offset));
        if (offset != 0) cs = cst[debug_state];
        // print cs info
        if (CollapsingHeader("Inpsect State")) {
            Text("frame: %llu, timestamp: %llu", cst[debug_state].frame_count, cst[debug_state].timestamp);
            FOR_OBJ(i) {
                char str[8]; snprintf(str, 8, "%d", i);
                if (CollapsingHeader(str)) {
                    current_state cs = cst[debug_state];
                    v2 unit_pos = cs.info[i].pos;
                    auto isl = [cei = cs.info[i].event_index](int ei){ return (ei == cei ? '*' : ' '); };
                    TextWrapped("%d - pos(%f %f)\nevent_index %d e[%c0] %d, e[%c1] %d, e[%c2] %d, e[%c3] %d\nreason %d\ntarget[%d]\nenergy[%f]\nst[%d]\ngopos[%f %f]\nteamid[%d]",
                        i, cs.info[i].pos.x, cs.info[i].pos.y, cs.info[i].event_index, isl(0), cs.info[i].last_events[0],
                        isl(1), cs.info[i].last_events[1], isl(2), cs.info[i].last_events[2], isl(3), cs.info[i].last_events[3],
                        cs.info[i].reason, cs.info[i].obj_id_target, cs.info[i].energy, cs.info[i].st, cs.info[i].go_target.x, cs.info[i].go_target.y, cs.info[i].team_id);

                }
            }
        }
    }

    Text("Server frame: %llu", cs.frame_count);
    Text("http_get average update %f ms", avg_server_update.last * 1000.f); // todo: threadsafe
    Text("Last Post Response: %s", post_data.data);

    static v2 go_pos = { 10.f, 20.f };
    SliderFloat2("Pos", (f32*)&go_pos, 0, 100);
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
        u32 id = UNIT_0 + com.team_id * MAX_UNIT;
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
}


#ifdef _WIN32
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    timeBeginPeriod(1); defer{ timeEndPeriod(1); }; // sleep granularity
    set_print_options({ "debug_client.txt" }); defer{ set_print_options({0}); };
#else
int main(void) {
#endif
    rend R = {};
    R.depth_test = false;
#ifdef _WIN32
    R.imgui_font_size = 28;
    R.imgui_font_file_ttf = "C:\\data\\rend\\imgui\\misc\\fonts\\Cousine-Regular.ttf";
#else
    R.imgui_font_size = 16;
    R.imgui_font_file_ttf = "imgui/misc/fonts/Cousine-Regular.ttf";
#endif
    R.init();

    cst = (current_state*)alloc(sizeof(current_state) * MAX_DEBUG_FRAMES);
    char post_data_buf[128] = {};
    post_data = { post_data_buf, 0, 128 };
    draw_data dd = init_rts_dd(R);

    start_update_thread();

    avg_data fps_avg = avg_init(0.5), frametime_avg = avg_init(0.5);
    i64 timer_start = tnow();
    current_state cs = {};
    while (!R.closed()) {
        ImGui::Begin("RTS");
        ImGui::TextWrapped("FPS: %f (avg %f), avg timeframe: %f, time: %f", 1 / R.fd, avg(1 / R.fd, &fps_avg), avg(R.fd * 1000.f, &frametime_avg), sec(tnow() - timer_start));
        
        { cs_mt.lock(); cs = cur_st; cs_mt.unlock(); }

        client_update(R, cs);
        R.clear({ 0.2f, 0.f, 0.f, 0.f });
        draw_rts(R, cs, tz_ef);
        R.submit_quads(&dd);

        ImGui::End();
        R.present();
    }
    R.cleanup();

    return 0;
}