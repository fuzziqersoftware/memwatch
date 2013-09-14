// parse_utils.h, by Martin Michelsen, 2012
// odds and ends used for messing with text and data

#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

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

#define FLAG_SHOW_WORDS    1
#define FLAG_SHOW_DWORDS   2
#define FLAG_SHOW_QWORDS   4
#define FLAG_SHOW_OWORDS   8

void change_color(int color, ...);

void print_data(unsigned long long address, const void* ds, const void* diff,
    unsigned long long data_size, int flags);

unsigned long long read_string_data(const char* in_buffer, void** vdata,
    void** vmask);
int read_ull(FILE* in, unsigned long long* value, int default_hex);
char* read_string_delimited(FILE* in, char delimiter, int consume_delim);
void trim_spaces(char* string);
const char* skip_word(const char* string, char delim);
char* get_word(const char* in, char delim);
int copy_quoted_string(const char* in, char** out);
int read_addr_size(const char* str, unsigned long long* addr,
    unsigned long long* size);
#endif // PARSE_UTILS_H
