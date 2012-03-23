#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#define FLAG_SHOW_WORDS    1
#define FLAG_SHOW_DWORDS   2
#define FLAG_SHOW_QWORDS   4
#define FLAG_SHOW_OWORDS   8

void CRYPT_PrintData(unsigned long long address, void* ds,
                     unsigned long long data_size, int flags);

unsigned long long read_stream_data(FILE* in, void** vdata);
int read_ull(FILE* in, unsigned long long* value, int default_hex);
char* read_string_delimited(FILE* in, char delimiter, int consume_delim);
void trim_spaces(char* string);
char* skip_word(char* string);

int read_string_data(const char* in_buffer, long in_size,
                     unsigned char* out_buffer);

#endif // PARSE_UTILS_H
