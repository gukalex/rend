/*
dependencies from the repo (or use conf.sh): 
    http.cpp http.h std.cpp std.h: git clone --depth 1 https://github.com/gukalex/rend
    httplib.h (used by http.cpp): curl https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h --output httplib.h
compile dependencies once: g++ -c http.cpp std.cpp -O3 -std=c++17
compile this demo: g++ client_min.cpp std.o http.o -O3 -std=c++17 -lpthread
run (with default parameters): ./a.out 
run with parameters: ./a.out team_id [HOST PORT]
*/

#include "std.h" // types, print, alloc/dealloc
#include "http.h" // http_get/post
#include "demo_rts.h" // constants and server structs

#include <stdlib.h> // atoi
#include <memory.h> // memcpy

using namespace rts; // demo_rts.h

//#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_IP "10.40.14.40"
#define DEFAULT_PORT 8080

const char* host = DEFAULT_IP;
int port = DEFAULT_PORT;
int team_id = 0; // 0 amogus, 1 stefan, 2 torbjorn, 3 pepe

current_state st; // state from the server
buffer_ex response_get; // http response buffer
buffer_ex response_post;

int get_closest_coffee_id(v2 unit_pos, f32 *distance_to_closest_coffee);
http_error post_command(update_command com);

int main(int argc, char** argv) {
    if (argc == 4) { // todo: validate input
        team_id = atoi(argv[1]);
        host = argv[2];
        port = atoi(argv[3]);
    } else if (argc == 2) {
        team_id = atoi(argv[1]);
    } else if (argc != 1) {
        print("usage: team_id [host] [port]");
        return 1;
    }

    response_get.data = (c8*)alloc(sizeof(st)); defer{ dealloc(response_get.data); };
    response_get.capacity = response_get.size = sizeof(st);
    int response_post_size = 128; // currently post response doesn't return anything useful (only text "Post Ok" or "Post Fail" if server received too many commands per 1 frame)
    response_post.data = (c8*)alloc(response_post_size); defer{ dealloc(response_post.data); }; //sizeof(update_response)
    response_post.capacity = response_post.size = response_post_size;

    while(true) {
        http_error err = http_get(host, port, "/state", &response_get);
        if (err == HTTP_ERROR) print("server error"); continue;
        memcpy(&st, response_get.data, sizeof(st));
        print("server tick: %llu", st.frame_count);

        update_command com[2] = {}; // client commands for the server (in this example we won't need more than 2 per frame)
        com[0].team_id = team_id; com[1].team_id = team_id;

        v2 spawn_pos = st.info[SPAWN_0 + team_id].pos;
        int first_unit_index = UNIT_0 + team_id * MAX_UNIT; // first_unit_index - index in the global object array
        for (int i = first_unit_index; i < first_unit_index + MAX_UNIT; i++) {
            v2 unit_pos = st.info[i].pos;
            int com_index = i - first_unit_index; // com_index - index in the local update_command array (0-9 and team_id are separate)
            // most basic and stupid AI implementation
            switch(st.info[i].st) { // eventually, you might want to implement your own client state (more advanced, perhaps) which is detached from the server
                case OBJ_STATE_UNIT_IDLE:
                    // do you have coffee?
                    if (st.info[i].obj_id_target == 0) { // no coffee, obj_id_target is an id of the object this unit carries 
                        // why are aren't working? - go grab a coffee
                        // check if there's a cup nearby
                        f32 distance_to_closest_coffee = ARENA_SIZE;
                        int coff_id = get_closest_coffee_id(unit_pos, &distance_to_closest_coffee);
                        if (distance_to_closest_coffee < EPS_GRAB) { // it is!
                            // grab it
                            com[0].update_mask[com_index] = true;
                            com[0].action[com_index] = ACTION_GRAB;
                            com[0].obj_id_target[com_index] = coff_id;
                            // and go home
                            com[1].update_mask[com_index] = true;
                            com[1].action[com_index] = ACTION_GO;
                            com[1].go_target[com_index] = spawn_pos;
                        } else if (coff_id) { // might be 0 in case you have all the coffee at your spawn
                            // go there
                            com[1].update_mask[com_index] = true;
                            com[1].action[com_index] = ACTION_GO;
                            com[1].go_target[com_index] = st.info[coff_id].pos;
                        }
                    } else { // you already got something!
                        // assuming you are at the spawn, let the coffee go :)
                        com[0].update_mask[com_index] = true;
                        com[0].action[com_index] = ACTION_PLACE; // alternatively the unit can fall asleep
                    }
                break;
                case OBJ_STATE_UNIT_SLEEPING: break; // if unit took the coffee (target_obj_id is not 0) its energy decreases and when its low it falls asleep.
                                                     // if you sleep at the spawn your energy recovers faster
                case OBJ_STATE_UNIT_WALKING: break; // keep going, todo: check the coffee you walking to isn't taken
                default: break; // OBJ_STATE_UNIT_TAKEN not implemented yet on server side
            }
        }
        // alternatively you can post command for each unit inside the switch above but that inefficient if you are gonna update everyone
        post_command(com[0]); // one of the commands might be empty (update_mask is false for every unit), but we don't care in this example.
        post_command(com[1]); // and a lot of things might go wrong with current server design (give your suggestions for the improvements)
                              // since we don't know if the first command is successful until we check the object's last_events for the event_type on the next frame 
                              // when server is done processing the commands (check frame_count in st)

        tsleep(nano(1/60.f)); // server isn't gonna update faster than that so we don't spam requests
    }

    return 0;
}

int get_closest_coffee_id(v2 unit_pos, f32 *distance_to_closest_coffee) {
    u32 target_id = 0;
    f32 min_len = 2.f * ARENA_SIZE;
    FOR_COFF(i) {
        v2 coffee_pos = st.info[i].pos;
        v2 spawn_pos = st.info[SPAWN_0 + team_id].pos;
        if (coffee_pos >= (spawn_pos - SPAWN_SIZE / 2.f) && coffee_pos <= (spawn_pos + SPAWN_SIZE / 2.f))
            continue; // ignore if it's already at the spawn
        f32 l = len(coffee_pos - unit_pos);
        if (l < min_len) {
            target_id = i;
            min_len = l;
        }
    }
    if (distance_to_closest_coffee)
        *distance_to_closest_coffee = min_len;
    return target_id;
}

http_error post_command(update_command com) { // todo: handle errors? do certain amount of tries
    return http_post(host, port, "/state", { (u8*)&com, sizeof(com) }, &response_post);
}
