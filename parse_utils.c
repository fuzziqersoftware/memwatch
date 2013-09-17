// parse_utils.c, by Martin Michelsen, 2012
// odds and ends used for messing with text and data

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include "parse_utils.h"
#include "search_data.h" // TODO: move bswap somewhere more accessible

extern int use_color;

// outputs control characters to the console to change color/formatting
void change_color(int color, ...) {

  char fmt[0x40] = "\033";
  int fmt_len = strlen(fmt);

  va_list va;
  va_start(va, color);
  do {
    fmt[fmt_len] = (fmt[fmt_len - 1] == '\033') ? '[' : ';';
    fmt_len++;
    fmt_len += sprintf(&fmt[fmt_len], "%d", color);
    color = va_arg(va, int);
  } while (color != FORMAT_END);
  va_end(va);

  fmt[fmt_len] = 'm';
  fmt[fmt_len + 1] = 0;

  printf("%s", fmt);
}

// prints data in a hex/ascii view to the console
void print_data(unsigned long long address, const void* ds, const void* diff,
    unsigned long long data_size, int flags) {

  if (data_size == 0)
    return;

  unsigned char* data_source = (unsigned char*)ds;
  unsigned char* diff_source = (unsigned char*)diff;
  unsigned long x, y, off = 0;
  char buffer[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  char diff_buffer[16];
  int space_factor = (flags & FLAG_SHOW_OWORDS) ? 15 :
                     (flags & FLAG_SHOW_QWORDS) ? 7 :
                     (flags & FLAG_SHOW_DWORDS) ? 3 :
                     (flags & FLAG_SHOW_WORDS) ? 1 :
                     0;

  printf("%016llX | ", address);
  for (x = 0; x < data_size; x++) {

    if (off == 16) {
      printf("| ");
      for (y = 0; y < 16; y++) {
        if (diff_buffer[y])
          change_color(FORMAT_FG_RED, FORMAT_END);
        if (buffer[y] < 0x20 || buffer[y] == 0x7F) {
          change_color(FORMAT_INVERSE, FORMAT_END);
          putc(' ', stdout);
          change_color(FORMAT_NORMAL, FORMAT_END);
        } else
          putc(buffer[y], stdout);
        if (diff_buffer[y])
          change_color(FORMAT_NORMAL, FORMAT_END);
      }
      printf("\n%016llX | ", address + x);
      off = 0;
    }

    buffer[off] = data_source[x];
    diff_buffer[off] = use_color && diff_source &&
                       (diff_source[x] != data_source[x]);
    if (diff_buffer[off])
      change_color(FORMAT_BOLD, FORMAT_FG_RED, FORMAT_END);
    if (!(off & space_factor))
      printf("%02X ", data_source[x]);
    else
      printf("%02X", data_source[x]);
    if (diff_buffer[off])
      change_color(FORMAT_NORMAL, FORMAT_END);

    off++;
  }
  buffer[off] = 0;
  for (y = 0; y < 16 - off; y++)
    printf("   ");
  printf("| ");
  for (y = 0; y < off; y++) {
    if (diff_buffer[y])
      change_color(FORMAT_FG_RED, FORMAT_END);
    if (buffer[y] < 0x20 || buffer[y] == 0x7F) {
      change_color(FORMAT_INVERSE, FORMAT_END);
      putc(' ', stdout);
      change_color(FORMAT_NORMAL, FORMAT_END);
    } else
      putc(buffer[y], stdout);
    if (diff_buffer[y])
      change_color(FORMAT_NORMAL, FORMAT_END);
  }
  printf("\n");
}

// reads an unsigned long long from the input string. defaults to decimal, but
// reads hex if the number is prefixed with "0x".
// reads binary  if the number is prefixed with "0b".
// returns 0 if an error occurred
int parse_ull(const char* in, unsigned long long* value, int default_hex) {
  if ((in[0] == '0') && ((in[1] == 'x') || (in[1] == 'X')))
    return sscanf(&in[2], "%llx", value);
  if ((in[0] == '0') && ((in[1] == 'b') || (in[1] == 'B'))) {
    *value = 0;
    in += 2;
    while (in[0] == '1' || in[0] == '2') {
      *value <<= 1;
      if (in[0] == 1)
        *value |= 1;
    }
    return 1;
  }
  return sscanf(in, default_hex ? "%llx" : "%llu", value);
}

// macros used by read_string_data to append to a buffer
#define expand(bytes) { \
  size += bytes; \
  *data = (unsigned char*)realloc(*data, size); \
  if (vmask) \
    *mask = (unsigned char*)realloc(*mask, size); \
}
#define write_byte(x) { \
  expand(1); \
  (*data)[size - 1] = x; \
  if (vmask) \
    (*mask)[size - 1] = 0xFF; \
}
#define write_short(x) { \
  expand(2); \
  *(uint16_t*)(*data + size - 2) = x; \
  if (vmask) \
    *(uint16_t*)(*mask + size - 2) = 0xFFFF; \
}
#define write_blank() { \
  expand(1); \
  (*mask)[size - 1] = 0; \
}

// parses the memwatch data format from a string
unsigned long long read_string_data(const char* in, void** vdata, void** vmask) {

  *vdata = NULL;
  if (vmask)
    *vmask = NULL;
  unsigned char** data = (unsigned char**)vdata;
  unsigned char** mask = (unsigned char**)vmask;

  int read, chr = 0;
  int string = 0, unicode_string = 0, high = 1;
  int filename = 0, filename_start;
  unsigned long size = 0;
  int endian = 0;
  while (in[0]) {
    read = 0;

    // if between quotes, read bytes to output buffer, unescaping
    if (string) {
      if (in[0] == '\"')
        string = 0;
      else if (in[0] == '\\') { // unescape char after a backslash
        if (!in[1])
          return size;
        if (in[1] == 'n') {
          write_byte('\n');
        } else if (in[1] == 'r') {
          write_byte('\r');
        } else if (in[1] == 't') {
          write_byte('\t');
        } else {
          write_byte(in[1]);
        }
        in++;
      } else
        write_byte(in[0]);
      in++;

    // if between single quotes, word-expand bytes to output buffer, unescaping
    } else if (unicode_string) {
      if (in[0] == '\'')
        unicode_string = 0;
      else if (in[0] == '\\') { // unescape char after a backslash
        if (!in[1])
          return size;
        if (in[1] == 'n') {
          write_short('\n');
        } else if (in[1] == 'r') {
          write_short('\r');
        } else if (in[1] == 't') {
          write_short('\t');
        } else {
          write_short(in[1]);
        }
        if (endian)
          bswap(&(*data)[size - 2], 2);
        in++;
      } else {
        write_short(in[0]);
        if (endian)
          bswap(&(*data)[size - 2], 2);
      }
      in++;

    // if between <>, read a file name, then stick that file into the buffer
    } else if (filename) {
      if (in[0] == '>') {
        filename = 0;
        write_byte(0); // null-terminate the filename
        // TODO: support <filename@offset:size> syntax

        // open the file, read it into the buffer, close the file
        FILE* f = fopen((char*)(*data + filename_start), "rb");
        if (!f) {
          if (data)
            free(data);
          return 0;
        }
        fseek(f, 0, SEEK_END);
        int file_size = ftell(f);
        size = filename_start + file_size;
        *data = realloc(*data, size);
        fseek(f, 0, SEEK_SET);
        fread((*data + filename_start), 1, file_size, f);
        fclose(f);
      } else
        write_byte(in[0]);
      in++;

    // ? is an unknown byte, but only if the caller wants a mask
    } else if (in[0] == '?' && vmask) {
      write_blank();
      in++;

    // $ changes the endian-ness
    } else if (in[0] == '$') {
      endian = !endian;
      in++;

    // # signifies a decimal number
    } else if (in[0] == '#') { // 8-bit
      unsigned long long value;
      in++;
      if (in[0] == '#') { // 16-bit
        in++;
        if (in[0] == '#') { // 32-bit
          in++;
          if (in[0] == '#') { // 64-bit
            in++;
            expand(8);
            parse_ull(in, (unsigned long long*)(&((*data)[size - 8])), 0);
            if (endian)
              bswap(&((*data)[size - 8]), 8);
          } else {
            expand(4);
            parse_ull(in, &value, 0);
            if (endian)
              bswap(&value, 4);
            *(int32_t*)(&((*data)[size - 4])) = value;
          }
        } else {
          expand(2);
          parse_ull(in, &value, 0);
          if (endian)
            bswap(&value, 2);
          *(int16_t*)(&((*data)[size - 2])) = value;
        }
      } else {
        expand(1);
        parse_ull(in, &value, 0);
        *(int8_t*)(&((*data)[size - 1])) = value;
      }
      if (in[0] == '-')
        in++;
      while (isdigit(in[0]))
        in++;

    // % is a float, %% is a double
    } else if (in[0] == '%') {
      in++;
      if (in[0] == '%') {
        in++;
        expand(8);
        double* value = (double*)(&((*data)[size - 8]));
        sscanf(in, "%lf", value);
        if (endian)
          bswap(value, 8);
      } else {
        expand(4);
        float* value = (float*)(&((*data)[size - 4]));
        sscanf(in, "%f", value);
        if (endian)
          bswap(value, 4);
      }
      if (in[0] == '-')
        in++;
      while (isdigit(in[0]) || (in[0] == '.'))
        in++;

    // anything else is a hex digit
    } else {
      if ((in[0] >= '0') && (in[0] <= '9')) {
        read = 1;
        chr |= (in[0] - '0');
      }
      if ((in[0] >= 'A') && (in[0] <= 'F')) {
        read = 1;
        chr |= (in[0] - 'A' + 0x0A);
      }
      if ((in[0] >= 'a') && (in[0] <= 'f')) {
        read = 1;
        chr |= (in[0] - 'a' + 0x0A);
      }
      if (in[0] == '\"')
        string = 1;
      if (in[0] == '\'')
        unicode_string = 1;
      if (in[0] == '<') {
        filename = 1;
        filename_start = size;
      }
      in++;
    }

    if (read) {
      if (high)
        chr = chr << 4;
      else {
        write_byte(chr);
        chr = 0;
      }
      high = !high;
    }
  }
  return size;
}

// reads a string from the given stream. return value must be free'd later.
char* read_string_delimited(FILE* in, char delimiter, int consume_delim) {
  char* string = NULL;
  int len = 0;
  do {
    len++;
    string = realloc(string, len);
    string[len - 1] = fgetc(stdin);
  } while (string[len - 1] != delimiter);
  string[len - 1] = 0;
  if (!consume_delim)
    ungetc(delimiter, in);
  return string;
}

// removes spaces from the beginning of a string
void trim_spaces(char* string) {
  if (!string)
    return;
  int x;
  for (x = 0; string[x] == ' ' && string[x]; x++);
  strcpy(string, &string[x]);
}

// finds the end of the word, returns a pointer to the first word after it
const char* skip_word(const char* in, char delim) {
  if (!in)
    return NULL;

  for (; *in != delim && *in; in++);
  if (!*in)
    return in;

  in++;
  for (; *in == delim; in++);
  return in;
}

// returns a copy of the first word in the string, which must be free'd later
char* get_word(const char* in, char delim) {
  if (!in)
    return NULL;

  const char* word_end = strchr(in, delim);
  return word_end ? strndup(in, word_end - in) : strdup(in);
}

// makes a copy of the quoted string given, taking care of escaped items. the
// quote character is the first character of in.
// returns the number of chars consumed in input, or -1 on error
int copy_quoted_string(const char* in, char** out) {
  if (!in || !out)
    return -1;
  if (!in[0])
    return 0;

  const char* in_orig = in;
  int out_size = 0;
  *out = NULL;
  char quote = in[0];
  for (in++; in[0] && in[0] != quote; in++) {
    out_size++;
    *out = (char*)realloc(*out, out_size * sizeof(char*));
    if (in[0] == '\\' && in[1]) {
      (*out)[out_size - 1] = in[1];
      in++;
    } else
      (*out)[out_size - 1] = in[0];
  }
  if (in[0] == quote)
    in++;

  return in - in_orig;
}

// reads addr:size (hex) or addr size (dec) from a string; returns the number of
// chars used up, or 0 if an error occurred
int read_addr_size(const char* str, unsigned long long* addr,
    unsigned long long* size) {

  // read address
  sscanf(str, "%llX", addr);

  // : means size is hex; space means it's decimal
  const char* next_col = skip_word(str, ':');
  const char* next_spc = skip_word(str, ' ');
  if (next_spc - next_col > 0 && next_col[0]) {
    sscanf(next_col, "%llX", size);
    return skip_word(next_col, ' ') - str;
  } else if (next_spc[0]) {
    sscanf(next_spc, "%llu", size);
    return skip_word(next_spc, ' ') - str;
  }
  return 0;
}
