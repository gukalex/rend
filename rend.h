#pragma once

#include "std.h" // for basic type aliases

struct GLFWwindow;
namespace KT {
enum {
    NONE = 0,
    IGNORE_IMGUI = 1
};
enum {
    KEY = 0,
    MBL,
    MBR,
    CTRL,
    SHIFT
};
}

constexpr u32 DATA_MAX_ELEM = 8; // todo: just size + pointers
struct indexed_buffer {
    u32 id; //vao...?
    u32 index_offset; // num of indices * sizeof(int)
    u32 vertex_count; // num of triangles * 3
};
struct draw_data {
    indexed_buffer ib;
    u32 prog; // todo: index and not direct opengl handle?
    u32 tex[DATA_MAX_ELEM] = {};
    m4 m = identity(), v = identity(), p = identity();
    // custom uniforms
};

struct dispatch_data {
    u32 prog = 0;
    u32 x = 1, y = 1, z = 1;
    u32 ssbo[DATA_MAX_ELEM] = {};
    //u32 ubo[DATA_MAX_ELEM] = {};
    // todo: image
};

struct resources {
    u32 buffer[DATA_MAX_ELEM]; // todo: just size + pointers
    u32 prog[DATA_MAX_ELEM];
    u32 texture[DATA_MAX_ELEM];
};

enum map_type {
    MAP_READ = 1, // GL_MAP_READ_BIT
    MAP_WRITE = 2 // GL_MAP_WRITE_BIT
};

struct quad_batcher {
    u32 max_quads = 100'000; // todo: revise or make the use of it more explicit 

    u32 vb_quad;
    u32 sb_quad_pos;
    u32 sb_quad_attr1;
    u32 sb_quad_indices;
    float* quad_pos; // vec2
    float* quad_attr1; // vec4
    u32 curr_quad_count = 0;
    u32 saved_count = 0; // for next_ib

    void init();
    void cleanup(); // todo: enum option to only cleanup cpu memory (for static mesh builders)
    indexed_buffer next_ib();
    u32 index_offset() { return curr_quad_count * 6 * sizeof(int); }
    u32 vertex_count() { return curr_quad_count * 6; }
    void upload(indexed_buffer* ib);

    void quad(v2 lb, v2 rt, v4 c); // 0-1
    void quad_a(v2 lb, v2 rt, v4 attr[4]);
    void quad_t(v2 lb, v2 rt, v2 attr);
    void quad_s(v2 center, f32 size, v4 c);
};

struct rend {
    iv2 wh = {1024, 1024};
    bool vsync = true;  // pre-init parameter, todo: make init parameter instead
    bool ms = true;     // multisample
    bool depth_test = true; // todo: put into draw state
    int debug = 1; // 1 - debug messages, 2 - GL_DEBUG_OUTPUT_SYNCHRONOUS; todo: enum debug_level
    bool save_and_load_win_params = true;
    const char* window_name = "rend";
    u32 imgui_font_size = 16;
    const char* imgui_font_file_ttf = nullptr;
    int max_progs = 32;    // 
    int max_textures = 32; //
    GLFWwindow* window;
    i64 curr_time; // time, nanosec
    i64 prev_time;
    float fd = 1 / 60.0f; // current frame delta

    u32 *progs;     // todo: revise
    int curr_progs; //
    u32 *textures;  //
    int curr_tex;   //

    draw_data dd = {};
    quad_batcher qb = {}; // todo: remove?

    const char* vs_quad = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aAttr;
        out vec4 vAttr;
        uniform mat4 rend_m, rend_v, rend_p;
        void main() {
           gl_Position = rend_p * rend_v * rend_m * vec4(aPos, 0.0, 1.0);
           vAttr = aAttr;
        })";
    const char* fs_quad = R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        void main() {
           FragColor = vAttr;
        })";
    const char* fs_quad_tex = R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        void main() {
            vec2 uv = vAttr.xy;
            FragColor.rgba = texture(rend_t0, uv).rgba;
            //todo: if depth test enabled do alpha test (or in a different shader)
        })";


    void init();
    bool closed(); // todo: rename/separate 
    void present();
    void cleanup();

    void win_size(iv2 wh); // set window size
    int key_pressed(u32 kt, u32 flags = KT::IGNORE_IMGUI);
    iv2 mouse();
    v2 mouse_norm();

    void clear(v4 c); // todo: push api
    //void submit(draw_data data); // todo: push api
    void dispatch(dispatch_data data); // compute // todo: push api
    void ssbo_barrier(); // todo: type

    u32 shader(const char* vs, const char* fs, const char* cs = 0, bool verbose = true);
    u32 texture(u8* data, int w, int h, int channel_count = 4); // todo: premultiply alpha option
    u32 texture(const char* filename);
    u32 ssbo(u64 size, void* init_data = 0, bool init_with_value = false, u32 default_u32_value = 0);
    void free_resources(resources res);
    void* map(u32 buffer, u64 offset, u64 size, map_type flags);
    void unmap(u32 buffer);

    void submit_quads(draw_data *dd); // updates indexed_buffer
    void submit(draw_data* dd, int dd_count);
};

// color constants
namespace C {
constexpr v4 BLACK = { 0.f,0.f,0.f,1.f }; // alpha = 1
constexpr v4 WHITE = { 1.f,1.f,1.f,1.f };
constexpr v4 RED   = { 1.f,0.f,0.f,1.f };
constexpr v4 GREEN = { 0.f,1.f,0.f,1.f };
constexpr v4 BLUE  = { 0.f,0.f,1.f,1.f };
constexpr v4 NICE  = { 0.95f, 0.62f, 0.61f, 1.f };
constexpr v4 NICE_DARK = { 77 / 255.f, 65 / 255.f, 109 / 255.f, 1.f };
}
