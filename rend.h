#pragma once

using f32 = float;
using u8 = unsigned char;
using u32 = unsigned int;
using i32 = int;
using i64 = long long int;

float RN();
float RNC(f32 b);

i64 tnow();
void tsleep(i64 nano);
inline float sec(i64 nanodiff) { return (float)(nanodiff / 1'000'000'000.0); }
inline i64 nano(float sec) { return (i64)(sec * 1'000'000'000.0); }
inline bool every(i64* state, i64 period) {
    i64 now = tnow();
    i64 diff = now - (*state + period);
    if (diff > 0) {
        *state = now - diff;
        return true;
    }
    return false;
}
struct avg_data { f32 last, accum; u32 count; i64 state, period; };
inline avg_data avg_init(float period_s = 1.0f) { return { 0.0, 0.0, 0, tnow(), nano(period_s) }; }
inline float avg(float val, avg_data* state) {
    state->count++;
    state->accum += val;
    if (state->count && every(&state->state, state->period)) {
        state->last = state->accum / state->count;
        state->accum = 0.f;
        state->count = 0;
    }
    return state->last;
}

struct v4 { float x, y, z, w; };
struct v2 { float x, y; };
inline v2 operator+(v2 r, v2 l) { return { r.x + l.x, r.y + l.y }; };
inline v2 operator-(v2 r, v2 l) { return { r.x - l.x, r.y - l.y }; };
inline v2 operator*(v2 r, v2 l) { return { r.x * l.x, r.y * l.y }; };
inline v2 operator*(v2 r, float f) { return { r.x * f, r.y * f }; };
inline v2 operator+(v2 r, float f) { return { r.x + f, r.y + f }; };
inline v2 operator-(v2 r, float f) { return { r.x - f, r.y - f }; };
inline v2 operator/(v2 r, float f) { return { r.x / f, r.y / f }; };

struct GLFWwindow;

struct draw_data { // todo: multiple
    u32 prog; // todo: index and not direct opengl handle?
    u32 tex;  //
};

struct rend {
    int w, h;
    bool vsync;            // init only
    int max_progs = 32;    // 
    int max_textures = 32; //
    GLFWwindow* window;
    i64 curr_time; // time, nanosec
    i64 prev_time;
    float fd = 1 / 60.0f; // current frame delta

    const char* vs_quad = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aAttr;
        out vec4 vAttr;
        void main() {
           gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
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
        })";
    u32 vb_quad;
    u32 sb_quad_pos;
    u32 sb_quad_attr1;
    u32 sb_quad_indices;
    u32 max_quads = 1'000'000; // per init
    float* quad_pos; // vec2
    float* quad_attr1; // vec4
    u32* quad_indices; // 2 triangles = 6
    u32 curr_quad_count = 0;
    draw_data default_quad_data; // first shader, no textures

    u32 *progs;
    int curr_progs;
    u32 *textures;
    int curr_tex;

    void init();
    u32 shader(const char* vs, const char* fs); // todo: error string
    u32 texture(u8* data, int w, int h, int channel_count = 4);
    u32 texture(const char* filename);
    void cleanup();
    void submit(draw_data data);
    void present();
    bool closed();
    void clear(v4 c);
    void quad(v2 lb, v2 rt, v4 c); // 0-1
    void quad_a(v2 lb, v2 rt, v4 attr[4]);
    void quad_t(v2 lb, v2 rt, v2 attr);
};
