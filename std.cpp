#include "std.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>


u8* alloc(u64 size) {
    return (u8*)malloc(size);
}

void dealloc(void* ptr) {
    free(ptr);
}

void print(const char* fmt, ...) {
    // todo: log to file when console isn't used
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void print_sl(const char* fmt, ...) {
    // todo: log to file when console isn't used
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

buffer read_file(const char* filename, int zero_padding_bytes) {
	buffer buf = {};

	FILE* file = fopen(filename, "rb");
	if (file != NULL) {
		fseek(file, 0, SEEK_END);
		buf.size = ftell(file);
		fseek(file, 0, SEEK_SET);

		buf.data = alloc(buf.size) + zero_padding_bytes;

		u64 read_count = fread(buf.data, buf.size, 1, file);
		ASSERT(read_count != 0); // or error state
        for (int i = 0; i < zero_padding_bytes; i++)
            buf.data[buf.size + i] = 0;
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