// parse_utils.h, by Martin Michelsen, 2012
// odds and ends used for messing with text and data

#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#define FLAG_SHOW_WORDS    1
#define FLAG_SHOW_DWORDS   2
#define FLAG_SHOW_QWORDS   4
#define FLAG_SHOW_OWORDS   8

void CRYPT_PrintData(unsigned long long address, const void* ds,
                     unsigned long long data_size, int flags);

unsigned long long read_stream_data(FILE* in, void** vdata);
unsigned long long read_string_data(const char* in_buffer, void** vdata);
int read_ull(FILE* in, unsigned long long* value, int default_hex);
char* read_string_delimited(FILE* in, char delimiter, int consume_delim);
void trim_spaces(char* string);
const char* skip_word(const char* string, char delim);
int copy_quoted_string(const char* in, char** out);
int read_addr_size(const char* str, unsigned long long* addr,
                   unsigned long long* size);
#endif // PARSE_UTILS_H
