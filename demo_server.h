#include "demo_rts.h"
#include "rend.h"
#include <mutex>
namespace demo_server {
using namespace rts;
#define FOR_OBJ(i) for (int i = 0; i < MAX_OBJ; i++)
#define FOR_COFF(i) for (int i = MAX_TEAMS * MAX_UNIT + MAX_TEAMS; i < MAX_TEAMS * MAX_UNIT + MAX_TEAMS + MAX_COFF; i++)
#define FOR_SPAWN(i) for (int i = MAX_TEAMS * MAX_UNIT; i < MAX_TEAMS * MAX_UNIT + MAX_TEAMS; i++)
#define SHADER_QUAD 5.0f
#define SHADER_COFF 4.0f
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
u64 score[MAX_TEAMS] = {0};
struct internal_object_state {
    object_state pobj; // todo: keep outside of this struct so I can easily copy it
    v2 go_pos;
    int event_index;
};
internal_object_state obj[MAX_OBJ];
draw_data dd;

// accessible upon unlocking the mutex 
#define MAX_COMMANDS 128
int unprocessed_commands = 0;
update_command commands[MAX_COMMANDS];
std::mutex cur_st_mt;
current_state cur_st = {};

void say_hi(void* data, u64* out_size, const char** out_content_type) {
    int size = snprintf((c8*)data, 128, "I'm tired\n\0");
    *out_content_type = "text/plain";
    *out_size = size; // todo: assert size // 1MB max at the moment
}

void get_state_callback(void* data, u64* out_size, const char** out_content_type) {
    current_state* st = (current_state*)data;
    *out_size = sizeof(cur_st); // todo: assert size // 1MB max at the moment
    *out_content_type = "application/octet-stream";
    cur_st_mt.lock(); defer {cur_st_mt.unlock();};
    memcpy(st, &cur_st, sizeof(cur_st));
}

void init(rend& R) {
    static server_callback cbk[] = { 
        {req_type::GET, "/hi", say_hi},
        {req_type::GET, "/state", get_state_callback},
    };
    start_server("0.0.0.0", 8080, ARSIZE(cbk), cbk);
    if (!dd.prog) {
        dd.p = ortho(0, ARENA_SIZE, 0, ARENA_SIZE);
        // todo: assert files exist
        R.textures[R.curr_tex++] = dd.tex[0] = R.texture("amogus.png");
        R.textures[R.curr_tex++] = dd.tex[1] = R.texture("din.jpg");
        R.textures[R.curr_tex++] = dd.tex[2] = R.texture("pool.png");
        R.textures[R.curr_tex++] = dd.tex[3] = R.texture("pepe.png");
        R.textures[R.curr_tex++] = dd.tex[4] = R.texture("coffee.png");
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
        // init units
        for(int i = 0; i < MAX_TEAMS * MAX_UNIT; i++) {
            obj[i].pobj.type = OBJ_UNIT;
            obj[i].pobj.st = OBJ_STATE_UNIT_IDLE;
            obj[i].pobj.obj_id = 1 + i; // 0 reserved
            obj[i].pobj.team_id = i / 10;
            v2 sl = spawn_loc[obj[i].pobj.team_id];
            obj[i].pobj.pos = { RNC(sl.x - SPAWN_SIZE / 2.f, sl.x + SPAWN_SIZE / 2.f), RNC(sl.y - SPAWN_SIZE / 2.f, sl.y + SPAWN_SIZE / 2.f) };
            obj[i].pobj.energy = MAX_UNIT_ENERGY;
            //last_events[2] = {};
            //target_obj_id;
        }
        // init spawn // todo: move at the last index last so we can render with transparency on the top
        //for (int i = MAX_TEAMS * MAX_UNIT; i < MAX_TEAMS * MAX_UNIT + MAX_TEAMS; i++) {
        FOR_SPAWN(i) {
            obj[i].pobj.type = OBJ_SPAWN;
            obj[i].pobj.st = OBJ_STATE_NONE;
            obj[i].pobj.obj_id = 1 + i; // 0 reserved
            obj[i].pobj.team_id = i - MAX_TEAMS * MAX_UNIT;
            obj[i].pobj.pos = spawn_loc[obj[i].pobj.team_id];
            obj[i].pobj.energy = MAX_SPAWN_ENERGY;
        }
        // init coff
        //for (int i = MAX_TEAMS * MAX_UNIT + MAX_TEAMS; i < MAX_TEAMS * MAX_UNIT + MAX_TEAMS + MAX_COFF; i++) {
        FOR_COFF(i) {
            obj[i].pobj.type = OBJ_COFF;
            obj[i].pobj.st = OBJ_STATE_COFF_IDLE;
            obj[i].pobj.obj_id = 1 + i; // 0 reserved
            obj[i].pobj.team_id = NO_TEAM_ID;
            float bo = ARENA_SIZE * 0.3; //border offset;
            obj[i].pobj.pos = { RNC(bo, ARENA_SIZE - bo ), RNC(bo, ARENA_SIZE - bo) };
            obj[i].pobj.energy = MAX_COFF_ENERGY;
        }
    }
}
void push_event(u32 obj_id, event_type ev) {
    if (obj[obj_id].event_index >= (MAX_LAST_EVENTS - 1)) {
        print("max ivents for obj %d", obj_id);
        obj[obj_id].event_index = 0;
    }
    obj[obj_id].pobj.last_events[obj[obj_id].event_index++] = ev;
}

u64 frame_count = 0;
void update(rend& R) {
    ImGui::Begin("Server"); defer{ ImGui::End(); };
    static v2 go_pos = { 50, 50 };
    ImGui::SliderFloat2("Go Pos", (f32*)&go_pos, 0, 100);
    ImGui::Text("[11]: state(%d), target(%d), energy(%f)", obj[11].pobj.st, obj[11].pobj.target_obj_id, obj[11].pobj.energy);
    v2 mov_dir = {};
    static bool keyboard_was_active = false;
    bool update_go = false;
    if (R.key_pressed('W')) mov_dir.y += 1.f;
    if (R.key_pressed('S')) mov_dir.y -= 1.f;
    if (R.key_pressed('A')) mov_dir.x -= 1.f;
    if (R.key_pressed('D')) mov_dir.x += 1.f;
    if (mov_dir != 0) {
        update_go = true;
        keyboard_was_active = true;
        mov_dir = norm(mov_dir); // fix diagonal
        go_pos = clamp(obj[11].pobj.pos + mov_dir * ARENA_SIZE, { 0,0 }, {ARENA_SIZE, ARENA_SIZE});
    }
    else if (keyboard_was_active == true /* and is moving */) {
        keyboard_was_active = false;
        // don't move further
        go_pos = obj[11].pobj.pos;
        update_go = true;
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
            f32 l = len(obj[i].pobj.pos - obj[id].pobj.pos);
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
    float fd = (R.fd > 1/60.f ? 1/60.f : R.fd); // sould be fixed so we don't freak out on spikes
    frame_count++;
    // process new commands, update under mutex
    if (unprocessed_commands > 0) {
        for (int i = 0; i < unprocessed_commands; i++) {
            update_command com = commands[i];
            // assert magic and version
            for (int j = 0; j < MAX_UNIT; j++) {
                if (com.update_mask[j]) {
                    u32 index = j + com.team_id * MAX_UNIT;
                    internal_object_state& ob = obj[index];
                    //if (ob.pobj.energy < UNIT_MIN_OPERATIONAL_ENERGY) {
                    if (ob.pobj.st == OBJ_STATE_UNIT_SLEEPING) {
                        ob.pobj.reason = REASON_OUT_OF_ENERGY;
                        continue;
                    }
                    switch (com.action[j]) {
                    case ACTION_GO:
                        if (ob.go_pos.x > ARENA_SIZE || ob.go_pos.x < 0 || ob.go_pos.y > ARENA_SIZE || ob.go_pos.y < 0) {
                            ob.pobj.reason = REASON_OUT_OF_BOUNDS;
                        } else {
                            ob.go_pos = com.go_target[j];
                            push_event(index, EVENT_GO_SUCCESS);
                            ob.pobj.st = OBJ_STATE_UNIT_WALKING;
                        }
                        break;
                    case ACTION_GRAB: {
                        u32 target_id = com.obj_id_target[j];
                        if (target_id == 0) {
                            ob.pobj.reason = REASON_ZERO_TARGET;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                            continue;
                        }
                        if (obj[target_id].pobj.st == OBJ_STATE_COFF_TAKEN) { // or != OBJ_IDLE or OBJ_TIRED
                            ob.pobj.reason = REASON_TARGET_TAKEN;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                            continue;
                        }
                        f32 l = len(obj[target_id].pobj.pos - ob.pobj.pos);
                        if (l < EPS_GRAB) {
                            // grab
                            push_event(index, EVENT_GRAB_AQUIRE_SUCCESS);
                            ob.pobj.target_obj_id = target_id;
                            obj[target_id].pobj.st = OBJ_STATE_COFF_TAKEN;
                        } else {
                            ob.pobj.reason = REASON_FAR_AWAY;
                            push_event(index, EVENT_GRAB_AQUIRE_FAIL);
                        }
                    } break;
                    case ACTION_PLACE:
                        break;
                    default: break;
                    }
                }
            }
        }
        unprocessed_commands = 0;
    }

    FOR_OBJ(i) {
        // decrease energy while holding the target
        u32 target_id = obj[i].pobj.target_obj_id;
        if (target_id) {
            obj[i].pobj.energy -= fd * GRAB_ENERGY_PER_S;
            if (obj[i].pobj.energy < UNIT_MIN_OPERATIONAL_ENERGY) {
                push_event(i, EVENT_GRAB_LOST);
                obj[i].pobj.reason = REASON_OUT_OF_ENERGY;
                obj[i].pobj.target_obj_id = 0;
                obj[target_id].pobj.st = OBJ_STATE_COFF_IDLE;
                obj[i].pobj.st = OBJ_STATE_UNIT_SLEEPING;
            }
        }
        switch (obj[i].pobj.st) {
            case OBJ_STATE_UNIT_WALKING: {
                v2 dir = obj[i].go_pos - obj[i].pobj.pos;
                if (len(dir) < EPS_GO) {
                    obj[i].pobj.st = OBJ_STATE_UNIT_IDLE;
                    // todo: clean obj[i].go_pos?
                } else {
                    // walk in the direction
                    obj[i].pobj.pos += norm(dir) * fd * UNIT_SPEED;
                    if (target_id) {
                        obj[target_id].pobj.pos += norm(dir) * fd * UNIT_SPEED;
                    }
                }
            } break;
            case OBJ_STATE_UNIT_SLEEPING: {
                obj[i].pobj.energy += fd * SLEEP_ENERGY_PER_S;
                if (obj[i].pobj.energy > MAX_UNIT_ENERGY) {
                    obj[i].pobj.st = OBJ_STATE_UNIT_IDLE;
                    push_event(i, EVENT_WOKE_UP);
                }
            } break;
            default: break;
        }
    }

    // update the state that will be shared with clients
    {
        cur_st_mt.lock(); defer{ cur_st_mt.unlock(); };
        cur_st.timestamp = tnow();
        cur_st.frame_count = frame_count;
        memcpy(cur_st.score, score, sizeof(score));
        cur_st.info_size = MAX_OBJ; // todo: construct range only visible to a specific client
        FOR_OBJ(i) {
            cur_st.info[i] = obj[i].pobj;
        }
    }

    FOR_OBJ(i) {
        switch (obj[i].pobj.type) {
        case OBJ_UNIT:
            R.quad_t(obj[i].pobj.pos - UNIT_SIZE / 2.f, obj[i].pobj.pos + UNIT_SIZE / 2.f, {0.f, (f32)obj[i].pobj.team_id});
            break;
        case OBJ_COFF:
            // todo: if grabbed - draw with transparent alpha
            R.quad_t(obj[i].pobj.pos - MAX_COFF_SIZE / 2.f, obj[i].pobj.pos + MAX_COFF_SIZE / 2.f, { 0.f, SHADER_COFF });
            break;
        case OBJ_SPAWN:
            R.quad(obj[i].pobj.pos - SPAWN_SIZE / 2.f, obj[i].pobj.pos + SPAWN_SIZE / 2.f, spawn_color[obj[i].pobj.team_id]);
            break;
        default: break;
        }
    }
    R.clear({ 0.1f, 0.3f, 0.1f, 0.f });
    R.submit(dd);
}
}