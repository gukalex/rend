#pragma once
#include "std.h" // type aliases + ASSERT

/*
server endpoints:
GET state - todo: pin=PIN
POST state - todo: pin=PIN

return update data immediately (and wait for it being processed) - slower responses, but synchronous
or poll updates from GET state - more async event handling (like debugging why unit coudn't move only after certain time)
technically both methods could be abstracted avay with right client interface

todo: join
todo: leave?id=N&pin=PIN

todo: text pop-ups for units :) 

*/

namespace rts {
// todo: init from file, change in imgui
constexpr f32 ARENA_SIZE = 100.f;
constexpr f32 SPAWN_SIZE = 10.f; // todo: use ARENA_SIZE
constexpr f32 UNIT_SIZE = 2.f;
constexpr f32 MAX_COFF_SIZE = 4.f;
constexpr f32 EPS_GO = 1.f;
constexpr f32 EPS_GRAB = MAX_COFF_SIZE;
constexpr f32 MAX_UNIT_ENERGY = 1000.f;
constexpr f32 MAX_SPAWN_ENERGY = 10000.f;
constexpr f32 MAX_COFF_ENERGY = 2000.f;
constexpr f32 UNIT_MIN_OPERATIONAL_ENERGY = 100.f;
constexpr f32 GRAB_ENERGY_PER_S = 300.f;
constexpr f32 SLEEP_ENERGY_PER_S = 100.f;
constexpr f32 SLEEP_ENERGY_AT_BASE_PER_S = 200.f;
constexpr f32 UNIT_SPEED = ARENA_SIZE / 10.f;
constexpr int MAX_UNIT = 10;
constexpr int MAX_TEAMS = 4;
constexpr int MAX_COFF = 10;
constexpr int MAX_OBJ = ((MAX_UNIT* MAX_TEAMS) + MAX_TEAMS + MAX_COFF);
constexpr int SPAWN_0 = (MAX_UNIT* MAX_TEAMS);
constexpr int COFF_0 = (MAX_UNIT* MAX_TEAMS + MAX_TEAMS);
constexpr u8 NO_TEAM_ID = 0xFF;
constexpr int MAX_LAST_EVENTS = 4;
constexpr int MAGIC_CURRENT_STATE = 0xBEEFFFFF;
constexpr int MAGIC_UPDATE_COMMAND = 0xBABEEEEE;
constexpr int VERSION_CURRENT_STATE = 1;
constexpr int VERSION_UPDATE_COMMAND = 1;

#define FOR_OBJ(i) for (int i = 0; i < MAX_OBJ; i++)
#define FOR_COFF(i) for (int i = COFF_0; i < COFF_0 + MAX_COFF; i++)
#define FOR_SPAWN(i) for (int i = SPAWN_0; i <SPAWN_0 + MAX_TEAMS; i++)

enum obj_type {
    OBJ_NONE,
    OBJ_UNIT, // both self and enemy
    OBJ_COFF,
    OBJ_SPAWN // both self and enemy
};
enum obj_state_type {
    OBJ_STATE_NONE,
    // todo: merge COFF and UNIT
    OBJ_STATE_UNIT_IDLE, //
    OBJ_STATE_UNIT_WALKING,
    OBJ_STATE_UNIT_SLEEPING,
    //OBJ_STATE_UNIT_GRAB,
    OBJ_STATE_UNIT_TAKEN, // :)
    OBJ_STATE_COFF_IDLE,
    OBJ_STATE_COFF_TAKEN,
    OBJ_STATE_COFF_IN_USE, // when at spawn
};
enum action_type {
    ACTION_NONE,
    ACTION_GRAB,
    ACTION_GO,
    ACTION_PLACE,
    ACTION_SLEEP,
    // action force wake up?
};
enum reason_type {
    REASON_NONE,
    REASON_OUT_OF_ENERGY,
    REASON_OUT_OF_BOUNDS,
    REASON_ZERO_TARGET,
    REASON_TARGET_TAKEN,
    REASON_FAR_AWAY,
};
enum event_type {
    EVENT_NONE,
    EVENT_GRAB_AQUIRE_SUCCESS,
    EVENT_GRAB_AQUIRE_FAIL, // out of reach, in own spawn
    EVENT_GRAB_LOST, // out of energy
    EVENT_PLACE_SUCCESS, // no fail state for that
    EVENT_NOTHING_TO_PLACE,
    EVENT_GO_SUCCESS,
    EVENT_GO_FAIL, // out of energy
    EVENT_WOKE_UP,
    EVENT_PUT_TO_SLEEP,
};
struct object_state {
    obj_type type = OBJ_NONE;
    obj_state_type st = OBJ_STATE_NONE;
    u32 obj_id = 0; // from 1 to MAX_OBJ + 1, 0 is reserved
    u8 team_id = NO_TEAM_ID; // NO_TEAM_ID for coffee
    v2 pos = {};
    // todo: add go pos or keep only on the server?
    // todo: bbox
    f32 energy = 0.f;
    // todo: remove last_events, mostly for debug and I don't know if there's a better approach
    event_type last_events[MAX_LAST_EVENTS] = {}; // todo: last event index, clear after every get state?
    reason_type reason = REASON_NONE;
    u32 target_obj_id = 0; // not empty if holding COFF or UNIT
};
struct current_state {
    int magic = MAGIC_CURRENT_STATE;
    int version = VERSION_CURRENT_STATE;
    u64 timestamp; // nanoseconds
    u64 frame_count; // todo: ensure fixed time
    //u64 events_processed
    u64 score[MAX_TEAMS] = {};
    i32 info_size = 0;  // number of visible objects in the radius // todo: specify radius
    object_state info[MAX_OBJ] = {}; // todo: make info per unit? so its visible who sees what?
};
struct update_command {
    int magic = MAGIC_UPDATE_COMMAND;
    int version = VERSION_UPDATE_COMMAND;
    u8 team_id = 0xFF;
    u8 update_mask[MAX_UNIT] = {};
    action_type action[MAX_UNIT] = {}; // todo: more then 1 action per unit (grab and go in one command)
    v2 go_target[MAX_UNIT] = {};
    u32 obj_id_target[MAX_UNIT] = {}; // grab/place obj id
};

inline int to_index(int id) { // obj[i].obj_id = i + 1;
    ASSERT(id != 0 && id <= MAX_OBJ); // 0 id is reserved
    return id - 1;
}

inline int to_id(int index) { // obj[i].obj_id = i + 1;
    ASSERT(index >= 0 && index < MAX_OBJ);
    return index + 1;
}

}