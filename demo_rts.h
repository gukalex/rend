#pragma once
#include "std.h" // type aliases

/*
server endpoints:
GET state - todo: pin=PIN
POST state - todo: pin=PIN

Use GET to obtain the state of all objects and POST to assign actions to the objects of your team.
Currently POST command doesn't return any useful data and you need to wait for the next frame to get new state 
and check events inside the objects to see if actions compleated successfully (like if you've sent GO command 
with out of bounds position and the unit did nothing - ), but a better approach is WIP.

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
constexpr int MAX_UNIT = 10; // per team
constexpr int MAX_TEAMS = 4;
constexpr int MAX_COFF = 10;
constexpr int UNIT_0 = 1;
constexpr int SPAWN_0 = UNIT_0 + (MAX_UNIT * MAX_TEAMS);
constexpr int COFF_0 = SPAWN_0 + MAX_TEAMS;
constexpr u8 NO_TEAM_ID = 0xFF;
constexpr int MAX_LAST_EVENTS = 4;
constexpr int MAGIC_CURRENT_STATE = 0xBEEFFFFF;
constexpr int MAGIC_UPDATE_COMMAND = 0xBABEEEEE;
constexpr int VERSION_CURRENT_STATE = 1;
constexpr int VERSION_UPDATE_COMMAND = 1;

constexpr int MAX_PORTAL = 4; // per team
constexpr f32 PORTAL_SIZE = 4.f; // per team
constexpr f32 EPS_PORTAL = PORTAL_SIZE; // per team
constexpr int PORTAL_0 = COFF_0 + MAX_COFF;

constexpr int MAX_OBJ = ((MAX_UNIT* MAX_TEAMS) + MAX_TEAMS + MAX_COFF + MAX_PORTAL) + 1; // + 1 for the empty object at index 0

#define FOR_OBJ(i) for (int i = 1; i < MAX_OBJ; i++)
#define FOR_UNIT(i) for (int i = UNIT_0; i < UNIT_0 + MAX_UNIT * MAX_TEAMS; i++)
#define FOR_SPAWN(i) for (int i = SPAWN_0; i < SPAWN_0 + MAX_TEAMS; i++)
#define FOR_COFF(i) for (int i = COFF_0; i < COFF_0 + MAX_COFF; i++)
#define FOR_PORTAL(i) for (int i = PORTAL_0; i < PORTAL_0 + MAX_PORTAL; i++)

enum obj_type {
    OBJ_NONE, // object 0
    OBJ_UNIT, // both self and enemy
    OBJ_COFF,
    OBJ_SPAWN, // both self and enemy
    OBJ_PORTAL
};
enum obj_state_type {
    OBJ_STATE_NONE,
    // todo: merge COFF and UNIT
    OBJ_STATE_UNIT_IDLE, //
    OBJ_STATE_UNIT_WALKING,
    OBJ_STATE_UNIT_SLEEPING,
    //OBJ_STATE_UNIT_GRAB,
    OBJ_STATE_UNIT_TAKEN, // not implemented :)
    OBJ_STATE_COFF_IDLE,
    OBJ_STATE_COFF_TAKEN,
    OBJ_STATE_COFF_IN_USE, // not implemented, idle works as well
};
enum action_type {
    ACTION_NONE,
    ACTION_GRAB,
    ACTION_GO,
    ACTION_PLACE,
    ACTION_SLEEP,
    ACTION_TELEPORT,
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
    EVENT_GO_FAIL, // out of bounds position
    EVENT_WOKE_UP,
    EVENT_PUT_TO_SLEEP,
    EVENT_TELEPORT_SUCCESS,
    EVENT_TELEPORT_FAIL,
};

struct object_state {
    obj_type type = OBJ_NONE;
    obj_state_type st = OBJ_STATE_NONE;
    u32 obj_id = 0; // from 1 to MAX_OBJ + 1, 0 is reserved
    u8 team_id = NO_TEAM_ID; // NO_TEAM_ID for coffee
    v2 pos = {};
    // todo: bbox, for now use UNIT_SIZE/SPAWN_SIZE/MAX_COFF_SIZE
    f32 energy = 0.f;
    // todo: remove last_events+reasons and move to update_response. But we probably need to keep events here for 
    // unit update logic which happens in the server main function 
    // - for instance when unit loses energy while moving coffee - EVENT_GRAB_LOST + EVENT_PUT_TO_SLEEP
    event_type last_events[MAX_LAST_EVENTS] = {}; // todo: last event index, clear after every get state?
    reason_type reason = REASON_NONE; // todo: array for last_events? (multiple events with multiple reasons)
    int event_index;
    v2 go_target; // todo: keep server only?
    u32 obj_id_target = 0; // not empty if holding COFF or UNIT
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
/* todo: add update_response struct - server can do action update in the http response and then the client won't need to wait until next frame to know if their action worked or not
WIP:
struct update_response {
    int magic = MAGIC_UPDATE_RESPONSE;
    int version = VERSION_UPDATE_RESPONSE;
    + debug info if action worked
    event_type events[MAX_UNIT];
    reason_type reasons[MAX_UNIT];
};
*/
}
