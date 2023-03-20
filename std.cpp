#include "std.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

// oh my
#include <chrono> // for tnow
#include <thread> // for tsleep
#include <math.h>

u8* alloc(u64 size) {
    return (u8*)malloc(size);
}

void dealloc(void* ptr) {
    free(ptr);
}

static print_options g_opts;
static FILE* g_print_file;
static char imgui_buffer[4 * 1024];

void set_print_options(print_options opts) {
    g_opts = opts;
    if (!g_opts.filename && g_print_file) {
        fclose(g_print_file); g_print_file = nullptr;
    }
    if (g_opts.filename)
        g_print_file = fopen(g_opts.filename, "wb");
}

void print(const char* fmt, ...) {
    if (g_opts.filename) {
        char temp[1024];
        va_list args;
        va_start(args, fmt);
        int pos = vsnprintf(temp, 1024, fmt, args);
        va_end(args);
        temp[pos++] = '\n';
        fwrite(temp, pos, 1, g_print_file);
        if (g_opts.always_flash) fflush(g_print_file);
    } // else {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void print_sl(const char* fmt, ...) {
    if (g_opts.filename) {
        char temp[1024];
        va_list args;
        va_start(args, fmt);
        int pos = vsnprintf(temp, 1024, fmt, args);
        va_end(args);
        fwrite(temp, pos, 1, g_print_file);
        if (g_opts.always_flash) fflush(g_print_file);
    } // else {
    // todo: log to file when console isn't used
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

buffer read_file(const char* filename, int zero_padding_bytes, bool add_padding_to_size) {
	buffer buf = {};

	FILE* file = fopen(filename, "rb");
	if (file != NULL) {
		fseek(file, 0, SEEK_END);
		buf.size = ftell(file);
		fseek(file, 0, SEEK_SET);

		buf.data = alloc(buf.size + zero_padding_bytes);

		u64 read_count = fread(buf.data, buf.size, 1, file);
		ASSERT(read_count != 0); // or error state
        for (int i = 0; i < zero_padding_bytes; i++)
            buf.data[buf.size + i] = 0;
        if (add_padding_to_size)
            buf.size += zero_padding_bytes;
		fclose(file);
	}

	return buf;
}

void write_file(const char* filename, buffer buf) {
    FILE* file = fopen(filename, "wb");
    if (file != NULL) {
        u64 write_count = fwrite(buf.data, 1, buf.size, file);
        ASSERT(write_count != 0);
        fclose(file);
    }
}

void _assert(const char* func, int line, const char* file) {
    fprintf(stderr, "assert in %s at %s: % d\n", func, file, line);
    // don't use write_file because of possible nested asserts
    FILE* f = fopen("last_assert.txt", "wb"); // todo: timestamp
    if (file != NULL) {
        c8 out_buf[1024];
        int msg_size = snprintf(out_buf, 1024, "assert in %s at %s:%d\n", func, file, line);
        fwrite(out_buf, 1, msg_size, f);
        fclose(f);
    }
    //#ifdef _WIN32
    //__debugbreak();
    //#endif
    abort();
}

m4 ortho(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
    return { 
        {2 / (r - l), 0, 0, -(r + l) / (r - l)},
        {0, 2 / (t - b), 0, -(t + b) / (t - b)},
        {0, 0, -2 /(f - n), -(f + n) / (f - n)},
        {0, 0, 0, 1} };
}

m4 identity() {
    return {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1} };
}

f32 len(v2 v) {
    return { sqrtf(v.x * v.x + v.y * v.y) };
}
v2 norm(v2 v) {
    return v / len(v);
}

float RN() { return rand() / (float)RAND_MAX; }
float RNC(f32 b) { return b + RN() * (1.f - 2.f * b); }
float RNC(f32 b, f32 e) { 
    f32 range = abs(e - b);
    f32 start = fminf(b, e);
    return start + range * RN();
}
int RNC(i32 b, i32 e) {
    f32 n = RN();
    i32 range = abs(e - b);
    i32 start = MIN(b, e);
    return start + (i32)(range * RN());
}

i64 tnow() { // nanoseconds since epoch
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}
void tsleep(i64 nano) {
    // todo: set timer granularity for windows
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}
