// parse_utils.h, by Martin Michelsen, 2012
// odds and ends used for messing with text and data

#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

#include <stdint.h>
#include <stdio.h>

#define FORMAT_END         (-1)
#define FORMAT_NORMAL      0
#define FORMAT_BOLD        1
#define FORMAT_UNDERLINE   4
#define FORMAT_BLINK       5
#define FORMAT_INVERSE     7
#define FORMAT_FG_BLACK    30
#define FORMAT_FG_RED      31
#define FORMAT_FG_GREEN    32
#define FORMAT_FG_YELLOW   33
#define FORMAT_FG_BLUE     34
#define FORMAT_FG_MAGENTA  35
#define FORMAT_FG_CYAN     36
#define FORMAT_FG_GRAY     37
#define FORMAT_FG_WHITE    38
#define FORMAT_BG_BLACK    40
#define FORMAT_BG_RED      41
#define FORMAT_BG_GREEN    42
#define FORMAT_BG_YELLOW   43
#define FORMAT_BG_BLUE     44
#define FORMAT_BG_MAGENTA  45
#define FORMAT_BG_CYAN     46
#define FORMAT_BG_GRAY     47
#define FORMAT_BG_WHITE    48

#define FLAG_USE_COLOR    1

static inline void bswap_int8_t(void* a) { }

static inline void bswap_int16_t(void* a) {
  *(uint16_t*)a = ((*(uint16_t*)a >> 8) & 0x00FF) |
                  ((*(uint16_t*)a << 8) & 0xFF00);
}

static inline void bswap_int32_t(void* a) {
  *(uint32_t*)a = ((*(uint32_t*)a >> 24) & 0x000000FF) |
                  ((*(uint32_t*)a >> 8)  & 0x0000FF00) |
                  ((*(uint32_t*)a << 8)  & 0x00FF0000) |
                  ((*(uint32_t*)a << 24) & 0xFF000000);
}

static inline void bswap_int64_t(void* a) {
  *(uint64_t*)a = ((*(uint64_t*)a >> 56) & 0x00000000000000FF) |
                  ((*(uint64_t*)a >> 40) & 0x000000000000FF00) |
                  ((*(uint64_t*)a >> 24) & 0x0000000000FF0000) |
                  ((*(uint64_t*)a >> 8)  & 0x00000000FF000000) |
                  ((*(uint64_t*)a << 8)  & 0x000000FF00000000) |
                  ((*(uint64_t*)a << 24) & 0x0000FF0000000000) |
                  ((*(uint64_t*)a << 40) & 0x00FF000000000000) |
                  ((*(uint64_t*)a << 56) & 0xFF00000000000000);
}

#define bswap_uint8_t bswap_int8_t
#define bswap_uint16_t bswap_int16_t
#define bswap_uint32_t bswap_int32_t
#define bswap_float bswap_int32_t
#define bswap_uint64_t bswap_int64_t
#define bswap_double bswap_int64_t

void bswap(void* a, int size);

void change_color(FILE* stream, int color, ...);

void print_data(FILE* stream, unsigned long long address, const void* ds,
    const void* diff, unsigned long long data_size, int flags);

unsigned long long read_string_data(const char* in_buffer, void** vdata,
    void** vmask);
void print_string_data(FILE* stream, void* data, void* mask,
    unsigned long long size);
int read_ull(FILE* in, unsigned long long* value, int default_hex);
char* read_string_delimited(FILE* in, char delimiter, int consume_delim);
void trim_spaces(char* string);
const char* skip_word(const char* string, char delim);
char* get_word(const char* in, char delim);
int copy_quoted_string(const char* in, char** out);
int read_addr_size(const char* str, unsigned long long* addr,
    unsigned long long* size);

void get_current_time_string(char* output);

int get_user_homedir(char* out, int out_len);

#endif // PARSE_UTILS_H
