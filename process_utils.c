// process_utils.c, by Martin Michelsen, 2012
// odds and ends used for messing with processes

#include <ctype.h>
#include <sys/time.h>

#include "parse_utils.h"
#include "process_utils.h"

// used by size_to_human_readable
#define KB_SIZE 1024ULL
#define MB_SIZE (KB_SIZE * 1024ULL)
#define GB_SIZE (MB_SIZE * 1024ULL)
#define TB_SIZE (GB_SIZE * 1024ULL)
#define PB_SIZE (TB_SIZE * 1024ULL)
#define EB_SIZE (PB_SIZE * 1024ULL)
#define ZB_SIZE (EB_SIZE * 1024ULL)
#define YB_SIZE (ZB_SIZE * 1024ULL)
#define HB_SIZE (YB_SIZE * 1024ULL)

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

// searches the process list for a given process
int ignore_match = -1;

typedef struct _find_pid_data {
  char name[PROCESS_NAME_LENGTH];
  pid_t pid;
  int matches_found;
} find_pid_data;

int find_pid_for_process_name(pid_t pid, const char* proc, void* param) {
  if (pid == ignore_match)
    return 0;
  
  int x;
  char compare_name[PROCESS_NAME_LENGTH];
  find_pid_data* data = (find_pid_data*)param;
  
  strcpy(compare_name, proc);
  for (x = 0; compare_name[x]; x++)
    compare_name[x] = tolower(compare_name[x]);
  int comp_len = strlen(compare_name) - strlen(data->name);
  if (comp_len < 0)
    return 0;
  
  for (x = 0; x <= comp_len; x++) {
    if (!strncmp(&compare_name[x], data->name, strlen(data->name))) {
      data->pid = pid;
      data->matches_found++;
      break;
    }
  }
  
  return 0;
}

// gets the pid of the process specified by name, optionally searching commands
// return: number of processes found (if not 1, then pid can't be trusted)
int getnamepid(const char* name, pid_t* pid, int commands) {

  int x;
  find_pid_data d;
  strcpy(d.name, name);
  for (x = 0; d.name[x]; x++)
    d.name[x] = tolower(d.name[x]);
  d.pid = 0;
  d.matches_found = 0;
  enumprocesses(find_pid_for_process_name, &d, commands);
  *pid = d.pid;
  return d.matches_found;
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

char* size_to_human_readable(uint64_t size, char* out) {

  if (size < KB_SIZE)
    sprintf(out, "%llu bytes", size);
  else if (size < MB_SIZE)
    sprintf(out, "%llu bytes (%.02f KB)", size, (float)size / KB_SIZE);
  else if (size < GB_SIZE)
    sprintf(out, "%llu bytes (%.02f MB)", size, (float)size / MB_SIZE);
  else if (size < TB_SIZE)
    sprintf(out, "%llu bytes (%.02f GB)", size, (float)size / GB_SIZE);
  else if (size < PB_SIZE)
    sprintf(out, "%llu bytes (%.02f TB)", size, (float)size / TB_SIZE);
  else if (size < EB_SIZE)
    sprintf(out, "%llu bytes (%.02f PB)", size, (float)size / PB_SIZE);
  else
    sprintf(out, "%llu bytes (%.02f EB)", size, (float)size / EB_SIZE);
  // a 64-bit int can't actually address a ZB or more

  return out;
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
  printf("address, end address, access flags\n");
  for (x = 0; x < map->numRegions; x++) {

    // print region info
    printf("%016llX %016llX %c%c%c/%c%c%c",
      map->regions[x].region._address,
      map->regions[x].region._size,
      (map->regions[x].region._attributes & VMREGION_READABLE) ? 'r' : '-',
      (map->regions[x].region._attributes & VMREGION_WRITABLE) ? 'w' : '-',
      (map->regions[x].region._attributes & VMREGION_EXECUTABLE) ? 'x' : '-',
      (map->regions[x].region._max_attributes & VMREGION_READABLE) ? 'r' : '-',
      (map->regions[x].region._max_attributes & VMREGION_WRITABLE) ? 'w' : '-',
      (map->regions[x].region._max_attributes & VMREGION_EXECUTABLE) ? 'x' : '-');
    if (!map->regions[x].data) {
      if (map->regions[x].error)
        printf(" [data not read, %d]", map->regions[x].error);
      else
        printf(" [data not read]");
    }
    printf("\n");

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
  char sizebuffer[0x40]; // doesn't need to be long...
  printf("%5lu regions, %s in total\n", map->numRegions,
         size_to_human_readable(total_bytes, sizebuffer));
  printf("%5lld regions, %s unread by memwatch\n", num_error,
         size_to_human_readable(total_error, sizebuffer));
  printf("%5lld regions, %s accessible\n", num_accessible,
         size_to_human_readable(total_accessible, sizebuffer));
  printf("%5lld regions, %s readable\n", num_readable,
         size_to_human_readable(total_readable, sizebuffer));
  printf("%5lld regions, %s writable\n", num_writable,
         size_to_human_readable(total_writable, sizebuffer));
  printf("%5lld regions, %s executable\n", num_executable,
         size_to_human_readable(total_executable, sizebuffer));
  printf("%5lld regions, %s inaccessible\n", num_inaccessible,
         size_to_human_readable(total_inaccessible, sizebuffer));
}

// prints a chunk of data, along with the process' name and the current time
void print_process_data(const char* processname, unsigned long long addr,
    void* read_data, void* diff_data, unsigned long long size) {

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
  print_data(addr, read_data, diff_data, size, 0);
  printf("\n");
}
