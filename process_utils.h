// process_utils.h, by Martin Michelsen, 2012
// odds and ends used for messing with processes

#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <sys/types.h>

#include "vmmap_data.h"

#define FLAG_SHOW_WORDS    1
#define FLAG_SHOW_DWORDS   2
#define FLAG_SHOW_QWORDS   4
#define FLAG_SHOW_OWORDS   8

#define PROCESS_NAME_LENGTH 0x100

int getpidname(pid_t pid, char* name, int namelen);
int getnamepid(const char* name, pid_t* pid, int commands);
int enumprocesses(int callback(pid_t pid, const char* proc, void* param),
                  void* param, int commands);

void print_region_map(VMRegionDataMap* map);
void print_process_data(const char* processname, unsigned long long addr,
    void* read_data, void* diff_data, unsigned long long size);
int write_file_to_process(const char* filename, unsigned long long size,
                          pid_t pid, unsigned long long addr);

#endif // PROCESS_UTILS_H
