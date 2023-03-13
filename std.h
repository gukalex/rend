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

#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a > b ? b : a)

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

struct buffer {
    u8* data;
    u64 size;
};

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
