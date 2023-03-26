/*
CONFIGURE:
(./conf.sh) 
download imgui, stb_image, httplib (glfw should be installed manually)

BUILD:
(./build.sh)
build object files for libraries we are not gonna touch (slow):
g++ -c glad.c stb_image.c -I. -Iimgui/backends -Iimgui imgui/imgui_unity.cpp -O3
build our demos (consider moving std.cpp/rend.cpp in the line above):
g++ server.cpp rend.cpp std.cpp -I. glad.o stb_image.o -Iimgui/backends -Iimgui imgui_unity.o -ldl -lglfw -lGLU -lGL -lXrandr -lX11 -lrt -O3 -o server
*/

#include "rend.h"
#include "imgui/imgui.h"
#include "demo_rts.h"
#include "rts_render.h"
#include "http.h"

#include <mutex>
#include <thread>

using namespace rts;

u64 frame_count = 0;
u64 score[MAX_TEAMS] = { 0 };
object_state obj[MAX_OBJ];

std::mutex tazer_mt;
tazer_ef tz_ef[MAX_TAZER_EF];

#define MAX_COMMANDS 256
std::mutex command_mt;
int unprocessed_commands = 0;
update_command commands[MAX_COMMANDS];
// can be accessed without command_mt mutex
int buf_unprocessed_commands = 0; // commands for temp copy
update_command buf_commands[MAX_COMMANDS]; // commands for temp copy

std::mutex cur_st_mt;
current_state cur_st = {};

void reset_rts_objects() {
    // frame_count = 0; ?
    const v2 spawn_loc[MAX_TEAMS] = {
        {10.f, 10.f}, {90.f, 90.f},
        {10.f, 90.f}, {90.f, 10.f},
    };
    const v2 portals_loc[MAX_PORTAL] = {
        {50.f, 10.f}, {50.f, 90.f},
        {10.f, 50.f}, {90.f, 50.f},
    };

    obj[0].type = OBJ_NONE; // special id
    // init units
    for (int i = UNIT_0; i < UNIT_0 + MAX_TEAMS * MAX_UNIT; i++) {
        obj[i].type = OBJ_UNIT;
        obj[i].st = OBJ_STATE_UNIT_IDLE;
        obj[i].obj_id = i;
        obj[i].team_id = (i - UNIT_0) / 10;
        v2 sl = spawn_loc[obj[i].team_id];
        obj[i].pos = { RNC(sl.x - SPAWN_SIZE / 2.f, sl.x + SPAWN_SIZE / 2.f), RNC(sl.y - SPAWN_SIZE / 2.f, sl.y + SPAWN_SIZE / 2.f) };
        obj[i].energy = MAX_UNIT_ENERGY;
        //last_events[2] = {};
        //target_obj_id;
    }
    // init spawn // todo: move at the last index last so we can render with transparency on the top
    FOR_SPAWN(i) {
        obj[i].type = OBJ_SPAWN;
        obj[i].st = OBJ_STATE_NONE;
        obj[i].obj_id = i;
        obj[i].team_id = i - SPAWN_0;
        obj[i].pos = spawn_loc[obj[i].team_id];
        obj[i].energy = MAX_SPAWN_ENERGY;
    }
    // init coff
    FOR_COFF(i) {
        obj[i].type = OBJ_COFF;
        obj[i].st = OBJ_STATE_COFF_IDLE;
        obj[i].obj_id = i;
        obj[i].team_id = NO_TEAM_ID;
        float bo = ARENA_SIZE * 0.3; //border offset;
        obj[i].pos = { RNC(bo, ARENA_SIZE - bo), RNC(bo, ARENA_SIZE - bo) };
        obj[i].energy = MAX_COFF_ENERGY;
    }
    // init portals
    FOR_PORTAL(i)
    {
        obj[i].type = OBJ_PORTAL;
        obj[i].st = OBJ_STATE_NONE;
        obj[i].obj_id = i;
        obj[i].team_id = NO_TEAM_ID;
        obj[i].pos = portals_loc[i - PORTAL_0];
        obj[i].energy = MAX_UNIT_ENERGY;
    }
}
void push_event(u32 obj_index, event_type ev) {
    obj[obj_index].last_events[obj[obj_index].event_index] = ev;
    obj[obj_index].event_index = (obj[obj_index].event_index + 1) % MAX_LAST_EVENTS;
}

void rts_update(float fd) {  // todo: i64 fd and always static
    frame_count++;

    { // process new commands, update under mutex
        command_mt.lock(); defer{ command_mt.unlock(); };
        if (unprocessed_commands > 0) {
            buf_unprocessed_commands = unprocessed_commands;
            memcpy(buf_commands, commands, unprocessed_commands * sizeof(update_command));
            unprocessed_commands = 0;
        }
    }
    if (buf_unprocessed_commands > 0) {
        for (int i = 0; i < buf_unprocessed_commands; i++) {
            update_command com = buf_commands[i];
            // assert magic and version
            for (int j = 0; j < MAX_UNIT; j++) {
                if (com.update_mask[j]) {
                    u32 index = UNIT_0 + j + com.team_id * MAX_UNIT;
                    object_state& ob = obj[index];
                    u32 obj_id_target = ob.obj_id_target;
                    if (ob.st == OBJ_STATE_UNIT_SLEEPING) {
                        ob.reason = REASON_OUT_OF_ENERGY;
                        continue;
                    }
                    switch (com.action[j]) {
                    case ACTION_GO:
                        if (ob.go_target.x > ARENA_SIZE || ob.go_target.x < 0 || ob.go_target.y > ARENA_SIZE || ob.go_target.y < 0) {
                            ob.reason = REASON_OUT_OF_BOUNDS;
                            push_event(index, EVENT_GO_FAIL);
                        }
                        else {
                            ob.go_target = com.go_target[j];
                            push_event(index, EVENT_GO_SUCCESS);
                            ob.st = OBJ_STATE_UNIT_WALKING;
                        }
                        break;
                    case ACTION_GRAB: {
                        u32 target_id = com.obj_id_target[j];
                        if (target_id == 0) {
                            ob.reason = REASON_ZERO_TARGET;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                            continue;
                        }
                        if (ob.obj_id_target != 0) {
                            ob.reason = REASON_DONT_YOU_HAVE_ENOUGH;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                            continue;
                        }
                        if (obj[target_id].st == OBJ_STATE_COFF_TAKEN) { // or != OBJ_IDLE or OBJ_TIRED
                            ob.reason = REASON_TARGET_TAKEN;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                            continue;
                        }
                        f32 l = len(obj[target_id].pos - ob.pos);
                        if (l < EPS_GRAB) {
                            // grab
                            push_event(index, EVENT_GRAB_AQUIRE_SUCCESS);
                            ob.obj_id_target = target_id;
                            obj[target_id].st = OBJ_STATE_COFF_TAKEN;
                        }
                        else {
                            ob.reason = REASON_FAR_AWAY;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                        }
                    } break;
                    case ACTION_PLACE: {
                        if (obj_id_target) {
                            push_event(index, EVENT_PLACE_SUCCESS);
                            obj[obj_id_target].st = OBJ_STATE_COFF_IDLE;
                        }
                        else {
                            push_event(index, EVENT_NOTHING_TO_PLACE);
                        }
                        ob.obj_id_target = 0;
                        ob.st = OBJ_STATE_UNIT_IDLE;
                    }
                                     break;
                    case ACTION_SLEEP: {
                        push_event(index, EVENT_PUT_TO_SLEEP);
                        ob.obj_id_target = 0;
                        if (obj_id_target)
                            obj[obj_id_target].st = OBJ_STATE_COFF_IDLE;
                        ob.st = OBJ_STATE_UNIT_SLEEPING;
                    }
                                     break;
                    case ACTION_TELEPORT: {
                        int portal = 0;
                        FOR_PORTAL(i)
                        {
                            f32 l = len(ob.pos - obj[i].pos);
                            if (l < EPS_PORTAL)
                                break;
                            portal++;
                        }
                        if (portal == MAX_PORTAL) {
                            push_event(index, EVENT_TELEPORT_FAIL);
                        }
                        else {
                            push_event(index, EVENT_TELEPORT_SUCCESS);
                            object_state& obj_portal = obj[portal + PORTAL_0];
                            v2 translate = ob.pos - obj_portal.pos;
                            int target = com.obj_id_target[j];
                            if (target < PORTAL_0 || target >= PORTAL_0 + MAX_PORTAL)
                                target = RNC(1, MAX_OBJ - 1);
                            ob.pos = obj[target].pos + translate;
                            if (obj_id_target)
                                obj[obj_id_target].pos = ob.pos;
                        }
                    }
                                        break;
                    case ACTION_TAZER: {
                        u32 target_id = com.obj_id_target[j];
                        bool tazed_yourself = false;
                        if (target_id == 0 || obj[target_id].st == OBJ_STATE_UNIT_SLEEPING) {
                            tazed_yourself = true;
                            ob.reason = obj[target_id].st == OBJ_STATE_UNIT_SLEEPING ? REASON_CRUEL : REASON_ZERO_TARGET;
                            push_event(index, EVENT_TAZER_YOURSERLF);
                            push_event(index, EVENT_PUT_TO_SLEEP);
                            ob.obj_id_target = 0;
                            if (obj_id_target)
                                obj[obj_id_target].st = OBJ_STATE_COFF_IDLE;
                            ob.st = OBJ_STATE_UNIT_SLEEPING;
                            ob.energy -= TAZER_ENERGY;
                            continue;
                        }
                        f32 l = len(obj[target_id].pos - ob.pos);
                        if (l < TAZER_RADIUS) { // do taze
                            {
                                tazer_mt.lock(); defer{ tazer_mt.unlock(); }; // todo: per unit?
                                tz_ef[index - UNIT_0].life = 20; // todo: config
                                tz_ef[index - UNIT_0].source_id = index;//&ob.pos;
                                tz_ef[index - UNIT_0].target_id = target_id;
                            }
                            push_event(index, EVENT_TAZER_SUCCESS);
                            ob.energy -= TAZER_ENERGY; // todo: depending on the length
                            push_event(target_id, EVENT_TAZED);
                            obj[target_id].st = OBJ_STATE_UNIT_SLEEPING;
                            obj[target_id].energy -= TAZER_ENERGY;
                            if (obj[target_id].obj_id_target) {
                                obj[obj[target_id].obj_id_target].st = OBJ_STATE_COFF_IDLE;
                                obj[target_id].obj_id_target = 0;
                            }
                        }
                        else {
                            tazed_yourself = true;
                            ob.reason = REASON_FAR_AWAY;
                            push_event(index, EVENT_TAZER_YOURSERLF);
                            push_event(index, EVENT_PUT_TO_SLEEP);
                            ob.obj_id_target = 0;
                            if (obj_id_target)
                                obj[obj_id_target].st = OBJ_STATE_COFF_IDLE;
                            ob.st = OBJ_STATE_UNIT_SLEEPING;
                            ob.energy -= TAZER_ENERGY;
                        }
                        if (tazed_yourself) {
                            tazer_mt.lock(); defer{ tazer_mt.unlock(); };
                            tz_ef[index - UNIT_0].life = 40;
                            tz_ef[index - UNIT_0].source_id = index;
                            tz_ef[index - UNIT_0].target_id = index;
                        }
                    }
                    default: break;
                    }
                }
            }
        }
        buf_unprocessed_commands = 0;
    }

    FOR_UNIT(i) {
        if (obj[i].energy < UNIT_MIN_OPERATIONAL_ENERGY && obj[i].st != OBJ_STATE_UNIT_SLEEPING) {
            push_event(i, EVENT_PUT_TO_SLEEP);
            obj[i].reason = REASON_OUT_OF_ENERGY;
            obj[i].st = OBJ_STATE_UNIT_SLEEPING;
        }
        // decrease energy while holding the target
        u32 target_id = obj[i].obj_id_target;
        if (target_id) {
            obj[i].energy -= fd * GRAB_ENERGY_PER_S;
            if (obj[i].energy < UNIT_MIN_OPERATIONAL_ENERGY) {
                push_event(i, EVENT_GRAB_LOST);
                obj[i].obj_id_target = 0;
                obj[target_id].st = OBJ_STATE_COFF_IDLE;
            }
        }
        switch (obj[i].st) {
        case OBJ_STATE_UNIT_WALKING: {
            v2 dir = obj[i].go_target - obj[i].pos;
            if (len(dir) < EPS_GO) {
                obj[i].st = OBJ_STATE_UNIT_IDLE;
                // todo: clean obj[i].go_pos?
                // todo: push event ARRIVED
            }
            else {
                // walk in the direction
                v2 new_pos = obj[i].pos + norm(dir) * fd * UNIT_SPEED;
                if (new_pos != clamp(new_pos, { 0,0 }, { ARENA_SIZE, ARENA_SIZE })) {
                    obj[i].st = OBJ_STATE_UNIT_IDLE;
                    push_event(i, EVENT_GO_FAIL);
                    obj[i].reason = REASON_OUT_OF_BOUNDS;
                }
                obj[i].pos = clamp(new_pos, { 0,0 }, { ARENA_SIZE, ARENA_SIZE });
                if (target_id) { // move attached object along with it
                    v2 obj_new_pos = obj[target_id].pos + norm(dir) * fd * UNIT_SPEED;
                    obj[target_id].pos = clamp(obj_new_pos, { 0,0 }, { ARENA_SIZE, ARENA_SIZE });
                }
            }
        } break;
        case OBJ_STATE_UNIT_SLEEPING: {
            f32 sleep_per_s = SLEEP_ENERGY_PER_S;
            u8 team_id = obj[i].team_id;
            v2 sp = obj[SPAWN_0 + team_id].pos;
            if (obj[i].pos >= (sp - SPAWN_SIZE / 2.f) && obj[i].pos <= (sp + SPAWN_SIZE / 2.f))
                sleep_per_s = SLEEP_ENERGY_AT_BASE_PER_S;
            if (obj[i].energy > MAX_UNIT_ENERGY) {
                obj[i].st = OBJ_STATE_UNIT_IDLE;
                push_event(i, EVENT_WOKE_UP);
            }
            else
                obj[i].energy += fd * sleep_per_s;
        } break;
        default: break;
        }
    }

    // score
    FOR_COFF(i) {
        v2 cp = obj[i].pos;
        if (obj[i].st == OBJ_STATE_COFF_TAKEN) continue;
        FOR_SPAWN(j) {
            v2 sp = obj[j].pos;
            if (cp >= (sp - SPAWN_SIZE / 2.f) && cp <= (sp + SPAWN_SIZE / 2.f)) {
                score[obj[j].team_id]++;
                obj[i].energy -= 1.f;
                if (obj[i].energy < 0.f) {
                    // respawn
                    float bo = ARENA_SIZE * 0.3; //border offset;
                    obj[i].pos = { RNC(bo, ARENA_SIZE - bo), RNC(bo, ARENA_SIZE - bo) };
                    obj[i].energy = MAX_COFF_ENERGY;
                }
            }
        }
    }

    { // update the state that will be shared with clients
        cur_st_mt.lock(); defer{ cur_st_mt.unlock(); };
        cur_st.timestamp = tnow();
        cur_st.frame_count = frame_count;
        memcpy(cur_st.score, score, sizeof(score));
        cur_st.info_size = MAX_OBJ; // todo: construct range only visible to a specific client
        memcpy(cur_st.info, obj, sizeof(obj));
    }
    {
        tazer_mt.lock(); defer{ tazer_mt.unlock(); }; // todo: do 
        for (int i = 0; i < MAX_TAZER_EF; i++) if (tz_ef[i].life > 0) tz_ef[i].life--;
    }
}

void start_render_thread() {
    std::thread render_th([]() {
        rend R = {};
        R.wh = { 1024, 1024 };
        R.save_and_load_win_params = true;
#ifdef _WIN32
        R.imgui_font_size = 28;
        R.imgui_font_file_ttf = "C:\\data\\rend\\imgui\\misc\\fonts\\Cousine-Regular.ttf";
#else
        R.imgui_font_size = 16;
        R.imgui_font_file_ttf = "imgui/misc/fonts/Cousine-Regular.ttf";
#endif
        R.init();
        draw_data dd = init_rts_dd(R);

        i64 timer_start = tnow();
        current_state state_to_render = {};
        i64 last_server_frame = -1;
        avg_data fps_avg = avg_init(0.5), frametime_avg = avg_init(0.5);
        while (!R.closed()) {
            ImGui::GetWindowViewport()->Flags |= ImGuiViewportFlags_TopMost;
            ImGui::Begin("RTS Server");
            ImGui::TextWrapped("FPS: %f (avg %f), avg timeframe: %f, time: %f", 1 / R.fd, avg(1 / R.fd, &fps_avg), avg(R.fd * 1000.f, &frametime_avg), sec(tnow() - timer_start));

            if (ImGui::Button("Reset")) reset_rts_objects(); // todo: threadsafe

            if (last_server_frame < (i64)frame_count) {
                cur_st_mt.lock(); defer{ cur_st_mt.unlock(); };
                memcpy(&state_to_render, &cur_st, sizeof(cur_st));
            }

            R.clear({ 0.1f, 0.3f, 0.1f, 0.f });
            draw_rts(R, state_to_render, tz_ef);
            R.submit(dd); // todo: inside draw_state?

            ImGui::End();
            R.present();
        }
        R.cleanup();
        exit(0); // close everything
    });
    render_th.detach();
}


void say_hi(http_response* resp, const char*, u64) {
    http_set_response(resp, "I'm tired", sizeof("I'm tired"), "text/plain");
}

void get_state_callback(http_response* resp, const char*, u64) {
    cur_st_mt.lock(); defer{ cur_st_mt.unlock(); };
    http_set_response(resp, (c8*)&cur_st, sizeof(cur_st), "application/octet-stream");
}

void get_tazer_ef_callback(http_response* resp, const char*, u64) {
    tazer_mt.lock(); defer{ tazer_mt.unlock(); };
    http_set_response(resp, (c8*)tz_ef, sizeof(tazer_ef)* MAX_TAZER_EF, "application/octet-stream");
}

void post_state_callback(http_response* resp, const char* post_data, u64 post_data_size) {
    const char* text_response = "Post OK"; // todo: binary error code
    if (post_data && post_data_size == sizeof(update_command)) {
        update_command* com = (update_command*)post_data;
        // todo: validate all the input
        bool invalid_data = false;
        invalid_data |= com->team_id >= MAX_TEAMS;
        for (int i = 0; i < MAX_UNIT; i++) invalid_data |= com->obj_id_target[i] >= MAX_OBJ;
        if (!invalid_data) {
            command_mt.lock(); defer{ command_mt.unlock(); };
            if (unprocessed_commands < MAX_COMMANDS) { // todo: handle it differently
                commands[unprocessed_commands] = *com;
                unprocessed_commands++;
            }
            else text_response = "Post Fail: maximum server commands";
        } else text_response = "Post Fail: update command contains invalid ids";
    }
    else {
        text_response = "Post Fail: invalid size or data";
    }
    http_set_response(resp, text_response, strlen(text_response), "text/plain");
}

#ifdef _WIN32
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    timeBeginPeriod(1); defer{ timeEndPeriod(1); }; // sleep granularity
    set_print_options({ "debug_server.txt" }); defer{ set_print_options({0}); };
#else
int main(void) {
#endif

    reset_rts_objects();

    server_callback cbk[] = {
        {REQUEST_GET, "/hi", say_hi},
        {REQUEST_GET, "/state", get_state_callback},
        {REQUEST_POST, "/state", post_state_callback},
        {REQUEST_GET, "/tazers", get_tazer_ef_callback},
    };
    start_server("0.0.0.0", 8080, ARSIZE(cbk), cbk);

    start_render_thread();

    // main simulation loop
    f32 server_tick_fd = 1 / 60.f; // todo: use non-float everywhere
    i64 server_tick_nano = nano(1.f) / 60;
    i64 next_frame = tnow() + server_tick_nano;
    while (true) {
        rts_update(server_tick_fd);

        i64 update_end = tnow();
        if (update_end < next_frame) { // todo: compensate timer resolution
            tsleep(next_frame - update_end);
            i64 after_sleep = tnow();
            print("sleep dur(ms): asked %f, got %f (diff %f - %s)", sec(next_frame - update_end)*1000.f, sec(after_sleep - update_end) * 1000.f, sec(next_frame - after_sleep) * 1000.f, (next_frame < after_sleep ? "oversleep" : "undersleep"));
        } else {
            print("long frame"); // todo: do something?
        }
        next_frame += server_tick_nano;
    }
   
    return 0;
}
