#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_utils.h"

// prints data! wheeee
void CRYPT_PrintData(unsigned long long address, void* ds,
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
  printf("| %s",buffer);
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

  long x;
  int in_ch, chr;
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

// reads an unsigned long long from the input file
// returns 0 if an error occurred
int parse_ull(char* in, unsigned long long* value, int default_hex) {
  if ((in[0] == '0') && ((in[1] == 'x') || (in[1] == 'X')))
    return sscanf(&in[2], "%llx", value);
  return sscanf(in, default_hex ? "%llx" : "%llu", value);
}

// reads a string from the given stream. return value must be free'd later
char* read_string_delimited(FILE* in, char delimiter) {
  char* string = NULL;
  int len = 0;
  do {
    len++;
    string = realloc(string, len);
    string[len - 1] = fgetc(stdin);
  } while (string[len - 1] != delimiter);
  string[len - 1] = 0;
  ungetc(delimiter, in);
  return string;
}

// removes spaces from the beginning of a string
void trim_spaces(char* string) {
  while (string[0] == ' ')
    strcpy(string, &string[1]);
}

// finds the end of the word, returns a pointer to the first word after
char* skip_word(char* in) {
  for (; *in != ' ' && *in; in++);
  if (!*in)
    return in;

  *in = 0;
  in++;
  for (; *in == ' '; in++);
  return in;
}

// like read_stream_data, but stops at the end of the string
int read_string_data(const char* in_buffer, long in_size,
                     unsigned char* out_buffer) {
  long x,size = 0;
  int chr;
  int read,string = 0,unicodestring = 0,high = 1;
  
  for (x = 0; x < in_size; x++) {
    read = 0;
    if (string) {
      if (in_buffer[x] == '\"') string = 0;
      else {
        out_buffer[size] = in_buffer[x];
        size++;
      }
    } else if (unicodestring) {
      if (in_buffer[x] == '\'') unicodestring = 0;
      else {
        out_buffer[size] = in_buffer[x];
        out_buffer[size + 1] = 0;
        size += 2;
      }
    } else if (in_buffer[x] == '#') {
      sscanf(&in_buffer[x + 1],"%ld",(long*)&out_buffer[size]);
      size += 4;
      while ((in_buffer[x + 1] >= '0') && (in_buffer[x + 1] <= '9')) x++;
    } else {
      if ((in_buffer[x] >= '0') && (in_buffer[x] <= '9')) {
        read = 1;
        chr |= (in_buffer[x] - '0');
      }
      if ((in_buffer[x] >= 'A') && (in_buffer[x] <= 'F')) {
        read = 1;
        chr |= (in_buffer[x] - 'A' + 0x0A);
      }
      if ((in_buffer[x] >= 'a') && (in_buffer[x] <= 'f')) {
        read = 1;
        chr |= (in_buffer[x] - 'a' + 0x0A);
      }
      if (in_buffer[x] == '\"') string = 1;
      if (in_buffer[x] == '\'') unicodestring = 1;
    }
    if (read) {
      if (high) chr = chr << 4;
      else {
        out_buffer[size] = chr;
        chr = 0;
        size++;
      }
      high = !high;
    }
  }
  return size;
}
