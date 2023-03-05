#pragma once

#define ASSERT(cond) do { if (!(cond)) { _assert(__func__, __LINE__, __FILE__); } } while(0)
#ifndef ARSIZE
#define ARSIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

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

void print(const char* fmt, ...); // print with a new line
void print_sl(const char* fmt, ...); // regular print, without a new line

buffer read_file(const char* filename, int zero_padding_bytes = 0, bool add_padding_to_size = false); // dealloc compatible
void write_file(const char* filename, buffer file); // todo: errors

void _assert(const char* func, int line, const char* file);
