#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"
#include "process_utils.h"
#include "memory_search.h"
#include "parse_utils.h"

// signal handler for ctrl+c: cancel operations

int* cancel_var = NULL;

void sigint(int signum) {
  if (cancel_var) {
    printf(" -- canceling operation\n");
    *cancel_var = 0;
    cancel_var = NULL;
  } else {
    printf("no operation to cancel - terminating\n");
    exit(0);
  }
}

// callbacks for process enumaration

// prints the process
int print_process(pid_t pid, const char* proc, void* param) {
  printf("%6u - %s\n", pid, proc);
  return 0;
}

// searches the process list for a given process
typedef struct _find_pid_data {
  char name[PROCESS_NAME_LENGTH];
  pid_t pid;
  int matches_found;
} find_pid_data;

int find_pid_for_process_name(pid_t pid, const char* proc, void* param) {
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
    }
  }

  return 0;
}

// prints the usage for this program
void print_usage() {
  printf(
"usage: sudo memwatch [options] pidname address <size>\n"
"yes, sudo is probably required.\n"
"pidname represents a process id or process name. the process name may be a\n"
"  partial name, in which case memwatch will search the running processes.\n"
"address and size may be in hexadecimal, prefixed with 0x.\n"
"size is necessary only if -w and -d are not specified. if -w is given without\n"
"  a size, then the file size is used.\n"
"\n"
"if size and -w are specified, the smaller of the file size and the given size\n"
"will be used; if -d is specified, the size is inferred from the given data.\n"
"\n"
"data is specified in immediate format. every pair of hexadecimal digits\n"
"  represents one byte, a 32-bit integer may be specified by preceding it with\n"
"  a #, ascii strings must be enclosed in \"double quotes\", and unicode strings\n"
"  in \'single quotes\'. any non-recognized characters are ignored. the\n"
"  endian-ness of the output depends on the endian-ness of the host machine: in\n"
"  this example, the machine is little-endian (x86).\n"
"example data string: 0304\"dark\"#-1_\'cold\'\n"
"resulting data: 03 04 64 61 72 6B FF FF FF FF 63 00 6F 00 6C 00 64 00\n"
"\n"
"options:\n"
"    -l          show a list of running processes, executable name only\n"
"    -L          show a list of running processes, including commands\n"
"    -p          pause the target process while reading or writing its memory\n"
"    -w<file>    write file to memory\n"
"    -d<data>    write given data to memory\n"
"    -g          do not loop; read/write once and exit\n"
"    -s<flag>    set display flags\n"
"    -i<time>    set update interval, in microseconds (default 1 second)\n"
"\n"
"example: read 32 bytes from 0x000FDB40 in process 498 every 5 seconds\n"
"    sudo memwatch 498 0x000FDB40 32 -i5000000\n"
"example: write 08000800 to 0x1058EA9C in process 7698 once\n"
"    sudo memwatch 7698 0x1058EA9C -d08000800 -g\n"
"example: write \"hello\" (with trailing \\0) to 0x1058EA9C in process 7698 once\n"
"    sudo memwatch 7698 0x1058EA9C -d\"hello\"00 -g\n"
"example: write data.bin to 0xF1096820 in process 933 every second\n"
"    sudo memwatch 933 0xF1096820 -wdata.bin\n"
"example: use variable finder on Firefox\n"
"    sudo memwatch firefox\n");
}

// main()
int main(int argc, char* argv[]) {

  // hello there
  printf("fuzziqer software memwatch\n\n");
  signal(SIGINT, sigint);

  // omg tons of variables
  mach_vm_address_t addr = 0;
  mach_vm_size_t size = 0;
  pid_t pid = 0;
  int loop = 1;
  int list_procs = 0;
  int list_commands = 0;
  int showflags = 0;
  int interval = 1000000;
  char* write_filename = NULL;
  void* write_data = NULL;
  int pause_during = 0;
  char processname[PROCESS_NAME_LENGTH] = {0};

  // parse command line args
  int x;
  for (x = 1; x < argc; x++) {

    // -g, --once-only: don't loop
    if (!strcmp(argv[x], "-g") || !strcmp(argv[x], "--once-only"))
      loop = 0;

    // -p, --pause: pause program while doing things to it
    else if (!strcmp(argv[x], "-p") || !strcmp(argv[x], "--pause"))
      pause_during = 1;

    // -s, --showflags: determine how to display data
    else if (!strncmp(argv[x], "-s", 2))
      showflags = atoi(&argv[x][2]);
    else if (!strncmp(argv[x], "--showflags=", 12))
      showflags = atoi(&argv[x][12]);

    // -i, --interval: determine how long to wait between repeated actions
    else if (!strncmp(argv[x], "-i", 2))
      interval = atoi(&argv[x][2]);
    else if (!strncmp(argv[x], "--interval=", 11))
      interval = atoi(&argv[x][11]);

    // -l, --list-processes: display a list of running processes
    else if (!strcmp(argv[x], "-l") || !strcmp(argv[x], "--list-processes"))
      list_procs = 1;

    // -L, --list-commands: like -l, but displays commands instead of processes
    else if (!strcmp(argv[x], "-L") || !strcmp(argv[x], "--list-commands"))
      list_commands = 1;

    // -w: write a file to memory
    else if (!strcmp(argv[x], "-w")) {
      if (write_data) {
        printf("can\'t use both -w and -d simultaneously\n");
        return (-2);
      }
      write_filename = &argv[x][2];

    // -d: write data to memory
    } else if (!strcmp(argv[x], "-d")) {
      if (write_filename) {
        printf("can\'t use both -d and -w simultaneously\n");
        return (-2);
      }
      if (size) {
        printf("can\'t use -d with a given size\n");
        return (-2);
      }
      size = strlen(&argv[x][2]);
      write_data = malloc(size);
      size = read_string_data(&argv[x][2], size, write_data);

    // first non-dash param: a process name or pid
    } else if (!pid && !processname[0]) {
      pid = atoi(argv[x]);
      if (!pid)
        strcpy(processname, argv[x]);

    // second non-dash param: an address
    } else if (!addr) {
      if ((argv[x][0] == '0') && ((argv[x][1] == 'x') || (argv[x][1] == 'X')))
        sscanf(&argv[x][2], "%lx", (unsigned long*)&addr);
      else
        sscanf(argv[x], "%lu", (unsigned long*)&addr);

    // third non-dash param: a size
    } else if (!size) {
      if (write_data) {
        printf("can\'t use -d with a given size\n");
        return (-2);
      }
      if ((argv[x][0] == '0') && ((argv[x][1] == 'x') || (argv[x][1] == 'X')))
        sscanf(&argv[x][2], "%lx", (unsigned long*)&size);
      else
        sscanf(argv[x], "%lu", (unsigned long*)&size);

    // any more non-dash params: error
    } else
      printf("unrecognized or unnecessary command line argument: %s\n", argv[x]);
  }

  // find pid for process name
  if (processname[0] && !pid) {
    find_pid_data d;
    strcpy(d.name, processname);
    for (x = 0; d.name[x]; x++)
      d.name[x] = tolower(d.name[x]);
    d.pid = 0;
    d.matches_found = 0;
    enumprocesses(find_pid_for_process_name, &d, 0);
    pid = d.pid;
    if (d.matches_found == 0) {
      printf("warning: no processes found by executable name; searching commands\n");
      d.matches_found = 0;
      enumprocesses(find_pid_for_process_name, &d, 1);
      pid = d.pid;
    }
    if (d.matches_found == 0) {
      printf("no processes found\n");
      return 0;
    }
    if (d.matches_found > 1) {
      printf("multiple processes found\n");
      return 0;
    }
  }

  // listing processes or commands?
  if (list_procs) {
    enumprocesses(print_process, NULL, 0);
    return 0;
  }
  if (list_commands) {
    enumprocesses(print_process, NULL, 1);
    return 0;
  }

  // some arguments missing? show usage
  if (!pid) {
    print_usage();
    return 0;
  }

  if (!addr && !size && !write_filename) {
    if (!pid) {
      printf("memory search mode requires a process id or name\n");
      return (-2);
    }
    if (getuid())
      printf("warning: memwatch likely will not work if not run as root\n");
    return memory_search(pid, pause_during);
  }

  if (getuid())
    printf("warning: memwatch likely will not work if not run as root\n");

  // writing a file? then read its data
  if (write_filename) {
    FILE* write_file = fopen(write_filename, "rb");
    if (!write_file) {
      printf("failed to open file: %s\n", write_filename);
      return (-1);
    }
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
    write_data = malloc((unsigned int)size);
    fread(write_data, size, 1, write_file);
    fclose(write_file);
  }

  // repeatedly read & write data
  int error, cont = 1;
  void* read_data = malloc((unsigned int)size);
  getpidname(pid, processname, 0x40);
  cancel_var = &cont;
  do {
    if (error = VMReadBytes(pid, addr, read_data, &size))
      print_process_data(processname, addr, read_data, size);
    else
      printf("failed to read data from process\n");
    if (write_data) {
      if (!VMWriteBytes(pid, addr, write_data, size))
        printf("failed to write data to process\n");
    }
    if (!loop)
      break;
    printf("\n");
    usleep(interval);
  } while (error && cont);
  free(read_data);
  if (write_data)
    free(write_data);

  return 0;
}
