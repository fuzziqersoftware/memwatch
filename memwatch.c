// memwatch.c, by Martin Michelsen, 2012
// the main app itself

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "vmmap.h"
#include "process_utils.h"
#include "memory_search.h"
#include "parse_utils.h"

// signal handler for ctrl+c: cancel operations first; if there are none to
// cancel, then quit

int* cancel_var = NULL;
int use_color = 1;

int recent_cancel = 0;
int recent_cancel_count = 0;

void sigint(int signum) {
  if (cancel_var) {
    printf(" -- canceling operation\n");
    *cancel_var = 0;
    cancel_var = NULL;
  } else {
    if (recent_cancel == time(NULL)) {
      if (recent_cancel_count > 1) {
        printf(" -- no operation to cancel - terminating\n");
        exit(0);
      } else
        recent_cancel_count++;
    } else {
      recent_cancel = time(NULL);
      recent_cancel_count = 0;
    }
  }
}

// callbacks for process enumaration

// prints the process pid and name
int print_process(pid_t pid, const char* proc, void* param) {
  printf("%6u - %s\n", pid, proc);
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

// entry point
int main(int argc, char* argv[]) {

  // hello there
  printf("fuzziqer software memwatch\n\n");
  signal(SIGINT, sigint);

  // only a few variables
  pid_t pid = 0;
  int list_procs = 0;
  int list_commands = 0;
  int showflags = 0;
  char processname[PROCESS_NAME_LENGTH] = {0};

  /*int num_commands = 0;
  char** commands = NULL; */

  // parse command line args
  int x;
  for (x = 1; x < argc; x++) {

    // -c, --no-color: don't use colors in terminal
    if (!strcmp(argv[x], "-c") || !strcmp(argv[x], "--no-color"))
      use_color = 0;

    // -s, --showflags: determine how to display data
    else if (!strncmp(argv[x], "-s", 2))
      showflags = atoi(&argv[x][2]);
    else if (!strncmp(argv[x], "--showflags=", 12))
      showflags = atoi(&argv[x][12]);

    // -l, --list-processes: display a list of running processes
    else if (!strcmp(argv[x], "-l") || !strcmp(argv[x], "--list-processes"))
      list_procs = 1;

    // -L, --list-commands: like -l, but displays commands instead of processes
    else if (!strcmp(argv[x], "-L") || !strcmp(argv[x], "--list-commands"))
      list_commands = 1;

    // first non-dash param: a process name or pid
    else if (!pid && !processname[0]) {
      pid = atoi(argv[x]);
      if (!pid)
        strcpy(processname, argv[x]);

    // all subsequent non-dash params: unnecessary parameters
    } else {
      // TODO: maybe someday we could take commands on the command line
      printf("warning: ignored excess argument: %s\n", argv[x]);
    }
  }

  // are we working on the kernel? (WOO DANGEROUS)
  int operate_on_kernel = 0;
  if (!strcmp(processname, "KERNEL")) {
    pid = 0;
    operate_on_kernel = 1;
  }

  // find pid for process name
  if (processname[0] && !pid && !operate_on_kernel) {
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
      ignore_match = getpid(); // don't match this process by command
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

  // pid missing?
  if (!pid && !operate_on_kernel) {
    printf("error: no process id or process name given\n");
    return (-2);
  }

  // warn user if pid is memwatch itself
  if (pid == getpid())
    printf("warning: memwatch is operating on itself\n");

  // warn user if not running as root
  if (getuid())
    printf("warning: memwatch likely will not work if not run as root\n");

  // finally, enter interactive interface
  return prompt_for_commands(pid);
}
