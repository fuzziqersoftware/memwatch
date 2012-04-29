// parse_utils.c, by Martin Michelsen, 2012
// odds and ends used for messing with text and data

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parse_utils.h"

// prints data! wheeee
void CRYPT_PrintData(unsigned long long address, const void* ds,
                     unsigned long long data_size, int flags) {

  unsigned char* data_source = (unsigned char*)ds;
  unsigned long x, y, off = 0;
  char buffer[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  printf("%016llX | ", address);
  for (x = 0; x < data_size; x++) {
    if (off == 16) {
      for (y = 0; y < 16; y++) {
        if (data_source[x + y - 16] < 0x20)
          buffer[y] = ' ';
        else
          buffer[y] = data_source[x + y - 16];
      }
      printf("| %s\n%016llX | ", buffer, address + x);
      off = 0;
    }
    if (flags & FLAG_SHOW_OWORDS) {
      if (off == 15)
        printf("%02X ", data_source[x]);
      else
        printf("%02X", data_source[x]);
    } else if (flags & FLAG_SHOW_QWORDS) {
      if (!((off + 1) & 7))
        printf("%02X ", data_source[x]);
      else
        printf("%02X", data_source[x]);
    } else if (flags & FLAG_SHOW_DWORDS) {
      if (!((off + 1) & 3))
        printf("%02X ", data_source[x]);
      else
        printf("%02X", data_source[x]);
    } else if (flags & FLAG_SHOW_WORDS) {
      if (!((off + 1) & 1))
        printf("%02X ", data_source[x]);
      else
        printf("%02X", data_source[x]);
    } else
      printf("%02X ", data_source[x]);
    off++;
  }
  buffer[off] = 0;
  for (y = 0; y < off; y++)
    buffer[y] = data_source[x - off + y];
  for (y = 0; y < off; y++)
    if (buffer[y] < 0x20)
      buffer[y] = ' ';
  for (y = 0; y < 16 - off; y++)
    printf("   ");
  printf("| %s\n",buffer);
}

// macros used by read_stream_data to append to a buffer
#define expand(bytes) \
  { *data = (unsigned char*)realloc(*data, size + bytes); \
  size += bytes; }
#define write_byte(x) \
  { expand(1); \
  (*data)[size - 1] = x; }
#define write_2byte(x, y) \
  { expand(2); \
  (*data)[size - 2] = x; \
  (*data)[size - 1] = y; }

// like read_string_data, but reads from a given stream, ending on a \n
unsigned long long read_stream_data(FILE* in, void** vdata) {

  *vdata = NULL;
  unsigned char** data = (unsigned char**)vdata;

  int in_ch, chr = 0;
  int read, string = 0, unicodestring = 0, high = 1;
  unsigned long size = 0;
  while ((in_ch = fgetc(in)) != EOF) {
    read = 0;
    if (string) {
      if (in_ch == '\"') string = 0;
      else write_byte(in_ch);
    } else if (unicodestring) {
      if (in_ch == '\'') unicodestring = 0;
      else write_2byte(in_ch, 0);
    } else if (in_ch == '\n') {
      ungetc('\n', stdin);
      break;
    } else if (in_ch == '#') {
      expand(4);
      fscanf(in, "%ld", (long*)(&((*data)[size - 4])));
    } else {
      if ((in_ch >= '0') && (in_ch <= '9')) {
        read = 1;
        chr |= (in_ch - '0');
      }
      if ((in_ch >= 'A') && (in_ch <= 'F')) {
        read = 1;
        chr |= (in_ch - 'A' + 0x0A);
      }
      if ((in_ch >= 'a') && (in_ch <= 'f')) {
        read = 1;
        chr |= (in_ch - 'a' + 0x0A);
      }
      if (in_ch == '\"') string = 1;
      if (in_ch == '\'') unicodestring = 1;
    }
    if (read) {
      if (high) chr = chr << 4;
      else {
        write_byte(chr);
        chr = 0;
      }
      high = !high;
    }
  }
  return size;
}

// like read_stream_data, but stops at the end of the string
unsigned long long read_string_data(const char* in, void** vdata) {
  
  *vdata = NULL;
  unsigned char** data = (unsigned char**)vdata;
  
  int chr = 0;
  int read, string = 0, unicodestring = 0, high = 1;
  unsigned long size = 0;
  while (in[0]) {
    read = 0;
    if (string) {
      if (in[0] == '\"') string = 0;
      else write_byte(in[0]);
      in++;
    } else if (unicodestring) {
      if (in[0] == '\'') unicodestring = 0;
      else write_2byte(in[0], 0);
      in++;
    } else if (in[0] == '#') {
      expand(4);
      in++;
      sscanf(in, "%ld", (long*)(&((*data)[size - 4])));
      while (isdigit(in[0]))
        in++;
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
      if (in[0] == '\"') string = 1;
      if (in[0] == '\'') unicodestring = 1;
      in++;
    }
    if (read) {
      if (high) chr = chr << 4;
      else {
        write_byte(chr);
        chr = 0;
      }
      high = !high;
    }
  }
  return size;
}

// reads an unsigned long long from the input file
// returns 0 if an error occurred
int parse_ull(const char* in, unsigned long long* value, int default_hex) {
  if ((in[0] == '0') && ((in[1] == 'x') || (in[1] == 'X')))
    return sscanf(&in[2], "%llx", value);
  return sscanf(in, default_hex ? "%llx" : "%llu", value);
}

// reads a string from the given stream. return value must be free'd later
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

// finds the end of the word, returns a pointer to the first word after
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
