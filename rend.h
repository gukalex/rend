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

struct rend {
    int w, h;
    bool vsync; // init only
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
        }
    )";
    const char* fs_quad = R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        void main() {
           FragColor = vAttr;
        }
    )";
    u32 prog_quad;
    u32 vb_quad;
    u32 sb_quad_pos;
    u32 sb_quad_attr1;
    u32 sb_quad_indices;
    u32 max_quads = 1'000'000; // per init
    float* quad_pos; // vec2
    float* quad_attr1; // vec4
    u32* quad_indices; // 2 triangles = 6
    u32 curr_quad_count = 0;

    void init();
    void cleanup();
    void present();
    bool closed();
    void clear(v4 c);
    void quad(v2 lb, v2 rt, v4 c); // 0-1
};
