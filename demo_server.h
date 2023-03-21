/*
TODO:
* validation (ids in the update command at least)
*/
#include "demo_rts.h"
#include "rend.h"
#include <mutex>
namespace demo_server {
using namespace rts;
#define SHADER_QUAD 5.0f
#define SHADER_COFF 4.0f
#define SHADER_PORT 6.0f
v2 spawn_loc[MAX_TEAMS] = {
    {10.f, 10.f},
    {90.f, 90.f},
    {10.f, 90.f},
    {90.f, 10.f},
};
v4 spawn_color[MAX_TEAMS] = {
    // w reserved for shader id
    {1, 0, 0, SHADER_QUAD},
    {0, 1, 0, SHADER_QUAD},
    {1, 0, 1, SHADER_QUAD},
    {1, 0, 1, SHADER_QUAD},
};

constexpr v2 portals_loc[MAX_PORTAL] = {
    {50.f, 10.f},
    {50.f, 90.f},
    {10.f, 50.f},
    {90.f, 50.f},
};

u64 score[MAX_TEAMS] = {0};
const char* team_names[MAX_TEAMS] = {"Amogus", "Stefan", "Torbjorn", "Pepe"};
object_state obj[MAX_OBJ];
draw_data dd;

#define MAX_COMMANDS 256
std::mutex command_mt;
int unprocessed_commands = 0;
update_command commands[MAX_COMMANDS];
// can be accessed without command_mt mutex
int buf_unprocessed_commands = 0; // commands for temp copy
update_command buf_commands[MAX_COMMANDS]; // commands for temp copy

std::mutex cur_st_mt;
current_state cur_st = {};

void say_hi(http_response* resp, const char*, u64) {
    http_set_response(resp, "I'm tired", sizeof("I'm tired"), "text/plain");
}

void get_state_callback(http_response* resp, const char*, u64) {
    cur_st_mt.lock(); defer {cur_st_mt.unlock();};
    http_set_response(resp, (c8*)&cur_st, sizeof(cur_st), "application/octet-stream");
}

void post_test(http_response* resp, const char* post_data, u64 post_data_size) {
    const char res_ok[] = "Post Ok"; 
    const char res_fail[] = "Post Fail";
    if (post_data) {
        v2 pos = *(v2*)post_data;
        print("post_data: %f %f", pos.x, pos.y);
        update_command com = {};
        com.team_id = 1;
        com.update_mask[1] = true;
        com.action[1] = ACTION_GO;
        com.go_target[1] = pos;

        bool fail = false;
        {
            command_mt.lock(); defer{ command_mt.unlock(); };
            if (unprocessed_commands < MAX_COMMANDS) {
                commands[unprocessed_commands] = com;
                unprocessed_commands++;
            } else 
                fail = true;
        }
        http_set_response(resp, (fail ? res_fail : res_ok), sizeof(fail ? res_fail : res_ok), "text/plain");
    } else {
        http_set_response(resp, res_fail, sizeof(res_fail), "text/plain");
    }
}


void post_state_callback(http_response* resp, const char* post_data, u64 post_data_size) {
    const char res_ok[] = "Post Ok\0"; // todo: binary
    const char res_fail[] = "Post Fail\0";
    if (post_data && post_data_size == sizeof(update_command)) {
        update_command* com = (update_command*)post_data;
        bool fail = false;
        {
            command_mt.lock(); defer{ command_mt.unlock(); };
            if (unprocessed_commands < MAX_COMMANDS) { // todo: handle it differently
                commands[unprocessed_commands] = *com;
                unprocessed_commands++;
            }
            else
                fail = true;
        }
        http_set_response(resp, (fail ? res_fail : res_ok), sizeof(fail ? res_fail : res_ok), "text/plain");
    }
    else {
        http_set_response(resp, res_fail, sizeof(res_fail), "text/plain");
    }
}

void init(rend& R) {
    static server_callback cbk[] = { 
        {REQUEST_GET, "/hi", say_hi},
        {REQUEST_GET, "/state", get_state_callback},
        {REQUEST_POST, "/post_test", post_test},
        {REQUEST_POST, "/state", post_state_callback},
    };
    start_server("0.0.0.0", 8080, ARSIZE(cbk), cbk);
    if (!dd.prog) {
        dd.p = ortho(0, ARENA_SIZE, 0, ARENA_SIZE);
        //const char* textures[] = {"star.png", "cloud.png", "heart.png", "lightning.png", "res.png"};
        //const char* textures[] = { "amogus.png", "din.jpg", "pool.png", "pepe.png", "coffee.png", "bkg.jpg" };
        const char* textures[] = { "amogus.png", "din.jpg", "pool.png", "pepe.png", "coffee.png", "portal1.png" };
        for (int i = 0; i < ARSIZE(textures); i++) R.textures[R.curr_tex++] = dd.tex[i] = R.texture(textures[i]);
        R.progs[R.curr_progs++] = dd.prog = R.shader(R.vs_quad, R"(#version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        uniform sampler2D rend_t1;
        uniform sampler2D rend_t2;
        uniform sampler2D rend_t3;
        uniform sampler2D rend_t4;
        uniform sampler2D rend_t5;
        void main() {
            vec2 uv = vAttr.xy;
            vec4 alpha = vec4(1,1,1,vAttr.z);
            if (vAttr.w == 0) FragColor.rgba = texture(rend_t0, uv) * alpha;
            else if (vAttr.w == 1) FragColor.rgba = texture(rend_t1, uv) * alpha;
            else if (vAttr.w == 2) FragColor.rgba = texture(rend_t2, uv) * alpha;
            else if (vAttr.w == 3) FragColor.rgba = texture(rend_t3, uv) * alpha;
            else if (vAttr.w == 4) FragColor.rgba = texture(rend_t4, uv) * alpha;
            else if (vAttr.w == 5) {
                FragColor.rgba = vec4(vAttr.xyz, 0.25 );
            } else if (vAttr.w == 6) {
                int index = int(vAttr.z);
                vec4 colors[4] = { vec4(0, 0, 1, 1), vec4(1, 0.5, 0, 1), vec4(1,0,0,1), vec4(1,1,0,1)};
                FragColor.rgba = texture(rend_t5, uv) * colors[index];
            }
            else
                FragColor.rgba = vec4(1, 0, 0, 1); // debug
        })");
        obj[0].type = OBJ_NONE; // special id
        // init units
        for(int i = UNIT_0; i < UNIT_0 + MAX_TEAMS * MAX_UNIT; i++) {
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
            obj[i].pos = { RNC(bo, ARENA_SIZE - bo ), RNC(bo, ARENA_SIZE - bo) };
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
}
void push_event(u32 obj_index, event_type ev) {
    obj[obj_index].last_events[obj[obj_index].event_index] = ev;
    obj[obj_index].event_index = (obj[obj_index].event_index + 1) % MAX_LAST_EVENTS;
}

u64 frame_count = 0;
u64 test_tp_portal = 0;
void update(rend& R) {
    // todo: flag to update in the background so I can switch between demos and expect the server to update
    ImGui::Begin("Server"); defer{ ImGui::End(); };
    static v2 go_pos = { 50, 50 };
    ImGui::SliderFloat2("Go Pos", (f32*)&go_pos, 0, 100);
    int test_id = UNIT_0 + MAX_UNIT + 1;
    ImGui::Text("[11?]: state(%d), target(%d), energy(%f), reason(%d)", obj[test_id].st, obj[test_id].obj_id_target, obj[test_id].energy, obj[test_id].reason);
    for (int i = 0; i < MAX_LAST_EVENTS; i++) {
        ImGui::Text("[11?]: event[%d]: %d", i, obj[test_id].last_events[i]);
    }
    FOR_SPAWN(i) {
        u8 team_id = obj[i].team_id;
        ImGui::Text("%s : %llu", team_names[team_id], score[team_id]);
    }
    
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
        go_pos = clamp(obj[test_id].pos + mov_dir * ARENA_SIZE, { 0,0 }, {ARENA_SIZE, ARENA_SIZE});
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
        commands[0] = com;
        unprocessed_commands = 1;
    }
    if (R.key_pressed('Q')) {
        update_command com = {};com.team_id=1;com.update_mask[1]=true;com.action[1]=ACTION_SLEEP;
        commands[0] = com; unprocessed_commands = 1;
    }
    if (ImGui::Button("Test Grab") || R.key_pressed(' ')) {
        update_command com = {};
        com.team_id = 1;
        com.update_mask[1] = true;
        u32 id = UNIT_0 + com.team_id * MAX_UNIT + 1;
        // find closest COFF
        if (obj[id].obj_id_target) {
            com.action[1] = ACTION_PLACE;    
            commands[0] = com;
            unprocessed_commands = 1;
        } else {
            com.action[1] = ACTION_GRAB;
            u32 target_id = 0;
            f32 min_len = ARENA_SIZE;
            FOR_COFF(i) {
                f32 l = len(obj[i].pos - obj[id].pos);
                if (l < min_len && l < EPS_GRAB) {
                    target_id = i;
                    min_len = l;
                }
            }
            if (target_id) {
                com.obj_id_target[1] = target_id;
                commands[0] = com;
                unprocessed_commands = 1;
            }
        }
    }
    if (ImGui::Button("Test Teleport")) {
        update_command com = {};
        com.team_id = 1;
        com.update_mask[1] = true;
        com.action[1] = rts::ACTION_TELEPORT;
        com.obj_id_target[1] = PORTAL_0 + test_tp_portal++ % MAX_PORTAL;
        commands[0] = com;
        unprocessed_commands = 1;
    }
    float fd = (R.fd > 1/60.f ? 1/60.f : R.fd); // sould be fixed so we don't freak out on spikes
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
                    //if (ob.energy < UNIT_MIN_OPERATIONAL_ENERGY) {
                    if (ob.st == OBJ_STATE_UNIT_SLEEPING) {
                        ob.reason = REASON_OUT_OF_ENERGY;
                        continue;
                    }
                    switch (com.action[j]) {
                    case ACTION_GO:
                        if (ob.go_target.x > ARENA_SIZE || ob.go_target.x < 0 || ob.go_target.y > ARENA_SIZE || ob.go_target.y < 0) {
                            ob.reason = REASON_OUT_OF_BOUNDS;
                            push_event(index, EVENT_GO_FAIL);
                        } else {
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
                        } else {
                            ob.reason = REASON_FAR_AWAY;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                        }
                    } break;
                    case ACTION_PLACE: {
                        if (obj_id_target) {
                            push_event(index, EVENT_PLACE_SUCCESS);
                            obj[obj_id_target].st = OBJ_STATE_COFF_IDLE;
                        } else {
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
                        } else {
                            push_event(index, EVENT_TELEPORT_SUCCESS);
                            object_state &obj_portal = obj[portal + PORTAL_0];
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
                    default: break;
                    }
                }
            }
        }
        buf_unprocessed_commands = 0;
    }

    FOR_UNIT(i) {
        // decrease energy while holding the target
        u32 target_id = obj[i].obj_id_target;
        if (target_id) {
            obj[i].energy -= fd * GRAB_ENERGY_PER_S;
            if (obj[i].energy < UNIT_MIN_OPERATIONAL_ENERGY) {
                push_event(i, EVENT_GRAB_LOST);
                push_event(i, EVENT_PUT_TO_SLEEP);
                obj[i].reason = REASON_OUT_OF_ENERGY;
                obj[i].obj_id_target = 0;
                obj[target_id].st = OBJ_STATE_COFF_IDLE;
                obj[i].st = OBJ_STATE_UNIT_SLEEPING;
            }
        }
        switch (obj[i].st) {
            case OBJ_STATE_UNIT_WALKING: {
                v2 dir = obj[i].go_target - obj[i].pos;
                if (len(dir) < EPS_GO) {
                    obj[i].st = OBJ_STATE_UNIT_IDLE;
                    // todo: clean obj[i].go_pos?
                    // todo: push event ARRIVED
                } else {
                    // walk in the direction
                    v2 new_pos = obj[i].pos + norm(dir) * fd * UNIT_SPEED;
                    if (new_pos != clamp(new_pos, {0,0}, {ARENA_SIZE, ARENA_SIZE})) {
                        obj[i].st = OBJ_STATE_UNIT_IDLE;
                        push_event(i, EVENT_GO_FAIL);
                        obj[i].reason = REASON_OUT_OF_BOUNDS;
                    }
                    obj[i].pos = clamp(new_pos,{0,0}, {ARENA_SIZE, ARENA_SIZE});
                    if (target_id) { // move attached object along with it
                        obj[target_id].pos += norm(dir) * fd * UNIT_SPEED;
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
                } else
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
                obj[i].energy -= 1.f ;
                if (obj[i].energy < 0.f) {
                    // respawn
                    float bo = ARENA_SIZE * 0.3; //border offset;
                    obj[i].pos = { RNC(bo, ARENA_SIZE - bo ), RNC(bo, ARENA_SIZE - bo) };
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

    R.clear({ 0.1f, 0.3f, 0.1f, 0.f });
    // draw grass:
    //R.quad_t({0,0}, {ARENA_SIZE,ARENA_SIZE}, {1.f, 6}); // 6 - bkg
    //R.submit(dd);

    FOR_OBJ(i) {
        switch (obj[i].type) {
        case OBJ_UNIT: {
            f32 alpha_level = (obj[i].st == OBJ_STATE_UNIT_SLEEPING ? 0.2f : 1.0f);
            R.quad_t(obj[i].pos - UNIT_SIZE / 2.f, obj[i].pos + UNIT_SIZE / 2.f, { alpha_level, (f32)obj[i].team_id });
        } break;
        case OBJ_COFF: {
            f32 alpha_level = (obj[i].st == OBJ_STATE_COFF_TAKEN ? 0.2f : 1.0f);
            R.quad_t(obj[i].pos - MAX_COFF_SIZE / 2.f, obj[i].pos + MAX_COFF_SIZE / 2.f, { alpha_level, SHADER_COFF });
        } break;
        case OBJ_SPAWN:
            R.quad(obj[i].pos - SPAWN_SIZE / 2.f, obj[i].pos + SPAWN_SIZE / 2.f, spawn_color[obj[i].team_id]);
            break;
        case OBJ_PORTAL: {
            //portal_colors[(i - PORTAL_0) / 2][(i - PORTAL_0) % 2]
            f32 color_index = i - PORTAL_0;
            R.quad_t(obj[i].pos - PORTAL_SIZE / 2.f, obj[i].pos + PORTAL_SIZE / 2.f, {color_index, SHADER_PORT});
        } break;
        default: break;
        }
    }

    //R.clear({ 0.1f, 0.3f, 0.1f, 0.f });
    R.submit(dd);
}
}