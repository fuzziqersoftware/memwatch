#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"
#include "vmmap_data.h"
#include "process_utils.h"

#include "parse_utils.h"

// gets the name for the specified process
int getpidname(pid_t pid, char* name, int namelen) {
  char command[0x80];
  sprintf(command, "ps -ax -c -o pid -o command | grep ^\\ *%u\\ | sed s/[0-9]*\\ //g",
          pid);
  FILE* run = popen(command, "r");
  if (!run)
    return 1;
  int ch, offset;
  for (offset = 0; (ch = fgetc(run)) != EOF && offset < namelen - 1; offset++)
    name[offset] = ch;
  name[offset] = 0;
  pclose(run);

  for (offset = 0; (offset < namelen) && name[offset]; offset++) {
    if ((name[offset] == '\\') && (name[offset + 1] == 'x')) {
      sscanf(&name[offset + 2], "%2hhX", &name[offset]);
      strcpy(&name[offset + 1], &name[offset + 4]);
    }
    if ((name[offset] == '\r') || (name[offset] == '\n')) {
      strcpy(&name[offset], &name[offset + 1]);
      offset--;
    }
  }
  
  return 0;
}

// calls the specified callback once for each process
int enumprocesses(int callback(pid_t pid, const char* proc, void* param),
                  void* param, int commands) {

  int namelen = 0x100;
  char name[namelen];
  FILE* run = popen(commands ? "ps -ax -o pid -o command | grep [0-9]" :
                               "ps -ax -c -o pid -o command | grep [0-9]", "r");
  if (!run)
    return 1;

  pid_t pid;
  int offset;
  while (!feof(run)) {
    fscanf(run, "%u", &pid);
    while ((name[0] = fgetc(run)) == ' ');
    ungetc(name[0], run);

    for (offset = 0; offset < namelen - 1; offset++) {
      name[offset] = fgetc(run);
      if (name[offset] == -1 || name[offset] == '\n') {
        name[offset] = 0;
        break;
      }
    }
    if (name[0])
      callback(pid, name, param);
  }

  pclose(run);
  return 0;
}

void print_region_map(VMRegionDataMap* map) {

  // initialize the counters
  unsigned long long total_error = 0, total_readable = 0, total_writable = 0,
      total_executable = 0, total_accessible = 0, total_inaccessible = 0,
      total_bytes = 0;
  unsigned long long num_error = 0, num_readable = 0, num_writable = 0,
      num_executable = 0, num_accessible = 0, num_inaccessible = 0;

  // enumerate the regions
  unsigned long x;
  printf("address, end address, size, access flags\n");
  for (x = 0; x < map->numRegions; x++) {

    // print region info
    printf("%016llX %016llX %c%c%c%s\n",
      map->regions[x].region._address,
      map->regions[x].region._size,
      (map->regions[x].region._attributes & VMREGION_READABLE) ? 'r' : '-',
      (map->regions[x].region._attributes & VMREGION_WRITABLE) ? 'w' : '-',
      (map->regions[x].region._attributes & VMREGION_EXECUTABLE) ? 'x' : '-',
      map->regions[x].data ? "" : " [data not read]");

    // increment the relevant counters
    if (!map->regions[x].data) {
      total_error += map->regions[x].region._size;
      num_error++;
    }
    if (map->regions[x].region._attributes & VMREGION_READABLE) {
      total_readable += map->regions[x].region._size;
      num_readable++;
    }
    if (map->regions[x].region._attributes & VMREGION_WRITABLE) {
      total_writable += map->regions[x].region._size;
      num_writable++;
    }
    if (map->regions[x].region._attributes & VMREGION_EXECUTABLE) {
      total_executable += map->regions[x].region._size;
      num_executable++;
    }
    if (map->regions[x].region._attributes & VMREGION_ALL) {
      total_accessible += map->regions[x].region._size;
      num_accessible++;
    } else {
      total_inaccessible += map->regions[x].region._size;
      num_inaccessible++;
    }
    total_bytes += map->regions[x].region._size;
  }

  // print the counters
  printf("%5lu regions (%lld bytes) in total\n", map->numRegions, total_bytes);
  printf("%5lld regions (%lld bytes) unread by memwatch\n", num_error,
         total_error);
  printf("%5lld regions (%lld bytes) accessible\n", num_accessible,
         total_accessible);
  printf("%5lld regions (%lld bytes) readable\n", num_readable, total_readable);
  printf("%5lld regions (%lld bytes) writable\n", num_writable, total_writable);
  printf("%5lld regions (%lld bytes) executable\n", num_executable,
         total_executable);
  printf("%5lld regions (%lld bytes) inaccessible\n", num_inaccessible,
         total_inaccessible);
}

// prints a chunk of data, along with the process' name and the current time
void print_process_data(const char* processname, unsigned long long addr,
                        void* read_data, unsigned long long size) {

  // get the current time
  struct timeval rawtime;
  struct tm cooked;
  const char* monthnames[] = {"January", "February", "March",
    "April", "May", "June", "July", "August", "September",
    "October", "November", "December"};
  gettimeofday(&rawtime, NULL);
  localtime_r(&rawtime.tv_sec, &cooked);

  // print the header
  printf("%s @ %016llX:%016llX // ", processname, addr, size);
  printf("%u %s %4u %2u:%02u:%02u.%03u\n", cooked.tm_mday,
         monthnames[cooked.tm_mon], cooked.tm_year + 1900,
         cooked.tm_hour, cooked.tm_min, cooked.tm_sec,
         rawtime.tv_usec / 1000);

  // and print the data
  CRYPT_PrintData(addr, read_data, size, 0);
  printf("\n");
}

// writes the contents of a file to a process' memory
int write_file_to_process(const char* filename, unsigned long long size,
                          pid_t pid, unsigned long long addr) {

  // open the file
  FILE* write_file = fopen(filename, "rb");
  if (!write_file) {
    printf("failed to open file: %s\n", filename);
    return (-1);
  }
  
  // get the file size and verify it
  fseek(write_file, 0, SEEK_END);
  unsigned long long file_size = ftell(write_file);
  fseek(write_file, 0, SEEK_SET);
  if (!size) {
    printf("given size is zero; using file size of %016llX\n", file_size);
    size = file_size;
  }
  if (file_size < size) {
    printf("file is shorter than given size; truncating size to %016llX\n", file_size);
    size = file_size;
  }
  
  // load the data from the file
  void* write_data = malloc((unsigned int)size);
  fread(write_data, size, 1, write_file);
  fclose(write_file);
  
  // and write it
  if (VMWriteBytes(pid, addr, write_data, size))
    printf("failed to write data to process\n");
  else
    printf("wrote %llu (%llX) bytes\n", size, size);
  free(write_data);

  return 0;
}
