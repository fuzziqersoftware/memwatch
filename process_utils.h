#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"
#include "vmmap_data.h"

#define FLAG_SHOW_WORDS    1
#define FLAG_SHOW_DWORDS   2
#define FLAG_SHOW_QWORDS   4
#define FLAG_SHOW_OWORDS   8

#define PROCESS_NAME_LENGTH 0x100

int getpidname(pid_t pid, char* name, int namelen);
int enumprocesses(int callback(pid_t pid, const char* proc, void* param),
                  void* param, int commands);

unsigned long long read_stream_data(FILE* in, void** vdata);
void print_region_map(VMRegionDataMap* map);
void print_process_data(const char* processname, unsigned long long addr,
                        void* read_data, unsigned long long size);
int write_file_to_process(const char* filename, unsigned long long size,
                          pid_t pid, unsigned long long addr);

#endif // MEMORY_UTILS_H
