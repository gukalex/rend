#pragma once

#define ASSERT(cond) do { if (!(cond)) { _assert(__func__, __LINE__, __FILE__); } } while(0)
#ifndef ARSIZE
#define ARSIZE(a) (sizeof(a)/sizeof(a[0]))
#endif
#define CONCAT(x, y) x##y
#define CONCAT_LINE_PREPROC(x, y) CONCAT(x, y)
#define CONCAT_LINE(x) CONCAT_LINE_PREPROC(x, __LINE__)

template<typename F> struct _defer { F f; _defer(F f) : f(f) {}; ~_defer() { f();} };
#define defer _defer CONCAT_LINE(_defer_) = [&]()

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

using u64 = unsigned long long;
using i64 = long long;
using u32 = unsigned int;
using i32 = int;
using u16 = unsigned short;
using i16 = short;
using u8 = unsigned char;
using i8 = char;
using c8 = char;
using f32 = float;

struct coroutine { u32 line; i64 time; };
#define CR(c) coroutine &_coro = c; switch(_coro.line)
#define CR_START() case 0: _coro.line = 0;
#define CR_YIELD() do { _coro.line = __LINE__; return; case __LINE__:; } while(0)
#define CR_YIELD_UNTIL(cond) while(!(cond)) { CORO_YIELD(); }
#define CR_WAIT(dur) do { if (_coro.time <= 0) _coro.time = tnow(); CR_YIELD_UNTIL(sec(tnow() - _coro.time) > dur); _coro.time = 0; } while (0)
#define CR_WHILE_START(dur) if (_coro.time <= 0) _coro.time = tnow(); while(sec(tnow() - _coro.time) < dur) {
#define CR_END CR_YIELD(); } _coro.time = 0;
#define CR_RESET() _coro = {};

struct v4 { float x, y, z, w; };
struct v2 { float x, y; 
v2 operator+=(v2 l) { x += l.x; y += l.y; return *this; };
operator bool() { return x != 0 || y != 0; };
bool operator==(v2 l) { return x == l.x && y == l.y; };
bool operator!=(v2 l) { return !(x == l.x && y == l.y); };
bool operator<=(v2 l) { return (x <= l.x && y <= l.y); };
bool operator>=(v2 l) { return (x >= l.x && y >= l.y); };
bool operator>(v2 l) { return (x > l.x && y > l.y); };
bool operator<(v2 l) { return (x < l.x && y < l.y); };
};
struct iv2 { int x, y; };
inline v2 operator+(v2 r, v2 l) { return { r.x + l.x, r.y + l.y }; };
inline v2 operator-(v2 r, v2 l) { return { r.x - l.x, r.y - l.y }; };
inline v2 operator*(v2 r, v2 l) { return { r.x * l.x, r.y * l.y }; };
inline v2 operator*(v2 r, float f) { return { r.x * f, r.y * f }; };
inline v2 operator+(v2 r, float f) { return { r.x + f, r.y + f }; };
inline v2 operator-(v2 r, float f) { return { r.x - f, r.y - f }; };
inline v2 operator/(v2 r, float f) { return { r.x / f, r.y / f }; };

struct m4 { v4 _0, _1, _2, _3; };

m4 ortho(f32 l, f32 r, f32 b, f32 t, f32 n = -1, f32 f = 1);
m4 identity();
f32 len(v2 v);
v2 norm(v2 v);
inline f32 clamp(f32 v, f32 b, f32 e) {
    return MAX(b, MIN(v, e));
}
inline v2 clamp(v2 v, v2 b, v2 e) {
    return { clamp(v.x, b.x, e.x), clamp(v.y, b.y, e.y) };
}
struct buffer {
    u8* data;
    u64 size;
};

// srand()? 
float RN(); // random normalized 
float RNC(f32 b); // random normalized with border(??) todo: remove, use the one below
float RNC(f32 b, f32 e); // random in the range from b to e
int RNC(i32 b, i32 e); // random int in the range

i64 tnow(); // current time since epoch in nanosec
void tsleep(i64 nano); // sleep 

inline float sec(i64 nanodiff) { return (float)(nanodiff / 1'000'000'000.0); } // convert to seconds
inline i64 nano(float sec) { return (i64)(sec * 1'000'000'000.0); } // convert to nanoseconds

inline bool every(i64* state, i64 period) { // should be called every frame to work
    i64 now = tnow();
    i64 diff = now - (*state + period);
    if (diff > 0) {
        *state = now - diff;
        return true;
    }
    return false;
}

// todo: why do I need this?
struct avg_data { f32 last, accum; u32 count; i64 state, period; };
inline avg_data avg_init(float period_s = 1.0f) { return { 0.0, 0.0, 0, tnow(), nano(period_s) }; }
inline float avg(float val, avg_data* state) { // should be called every frame to work
    state->count++;
    state->accum += val;
    if (state->count && every(&state->state, state->period)) {
        state->last = state->accum / state->count;
        state->accum = 0.f;
        state->count = 0;
    }
    return state->last;
}

u8* alloc(u64 size);
void dealloc(void* ptr);

struct print_options {
    const char* filename = 0;
    bool always_flash = false; // slow
    bool imgui = false;
};
void set_print_options(print_options opts); // set filename to 0 to close debug file
void print(const char* fmt, ...); // print with a new line
void print_sl(const char* fmt, ...); // regular print, without a new line

buffer read_file(const char* filename, int zero_padding_bytes = 0, bool add_padding_to_size = false); // dealloc compatible
void write_file(const char* filename, buffer file); // todo: errors

void _assert(const char* func, int line, const char* file);
