// memwatch.c, by Martin Michelsen, 2012
// the main app itself

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "memory_search.h"
#include "process_utils.h"

int use_color = 1;

// signal handler for ctrl+c: cancel operations first; if there are none to
// cancel, then quit

int* cancel_var = NULL;
int recent_cancel = 0;
int recent_cancel_count = 0;

extern int ignore_match;

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

// prints the process pid and name
// called bu enum_processes, where invoked below
int print_process(pid_t pid, const char* proc, void* param) {
  printf("%6u - %s\n", pid, proc);
  return 0;
}

void print_usage() {
  printf("usage: memwatch [ -l | -L ]\n");
  printf("       memwatch [ -c ] [ -f ] pid_or_name\n");
  printf("see `man memwatch` for more information\n");
}

// entry point
int main(int argc, char* argv[]) {

  // hello there
  printf("fuzziqer software memwatch\n\n");

  // install our ctrl+c handler
  signal(SIGINT, sigint);

  // only a few variables
  pid_t pid = 0;
  int list_procs = 0;
  int list_commands = 0;
  int showflags = 0;
  int freeze_while_operating = 1;
  uint64_t max_results = 1024 * 1024 * 1024; // approx. 1 billion
  char process_name[PROCESS_NAME_LENGTH] = {0};
  char *input_command = NULL;
  int input_command_len = 0;

  // parse command line args
  int x;
  for (x = 1; x < argc; x++) {

    // -c, --no-color: don't use colors in terminal output
    if (!strcmp(argv[x], "-c") || !strcmp(argv[x], "--no-color"))
      use_color = 0;

    // -f, --no-freeze: don't freeze target while operating on it
    if (!strcmp(argv[x], "-f") || !strcmp(argv[x], "--no-freeze"))
      freeze_while_operating = 0;

    // -s, --showflags: determine how to display data
    else if (!strncmp(argv[x], "-s", 2))
      showflags = atoi(&argv[x][2]);
    else if (!strncmp(argv[x], "--showflags=", 12))
      showflags = atoi(&argv[x][12]);

    // -n, --max-results: set maximum number of results
    else if (!strncmp(argv[x], "-n", 2))
      sscanf(&argv[x][2], "%llu", &max_results);
    else if (!strncmp(argv[x], "--max-results=", 14))
      sscanf(&argv[x][14], "%llu", &max_results);

    // -l, --list-processes: display a list of running processes
    else if (!strcmp(argv[x], "-l") || !strcmp(argv[x], "--list-processes"))
      list_procs = 1;

    // -L, --list-commands: like -l, but displays commands instead of processes
    else if (!strcmp(argv[x], "-L") || !strcmp(argv[x], "--list-commands"))
      list_commands = 1;

    // first non-dash param: a process name or pid
    else if (!pid && !process_name[0]) {
      pid = atoi(argv[x]);
      if (!pid)
        strcpy(process_name, argv[x]);

    // all subsequent non-dash params: unnecessary parameters
    } else {

      int token_len = strlen(argv[x]);
      input_command = (char*)realloc(input_command, input_command_len + token_len + 2);
      input_command[input_command_len] = ' ';
      strcpy(&input_command[input_command_len + 1], argv[x]);
      input_command_len += (token_len + 1);
    }
  }

  // are we working on the kernel? (WOO DANGEROUS)
  int operate_on_kernel = 0;
  if (!strcmp(process_name, "KERNEL")) {
    pid = 0;
    operate_on_kernel = 1;
  }

  // listing processes or commands?
  if (list_procs) {
    enum_processes(print_process, NULL, 0);
    return 0;
  }
  if (list_commands) {
    enum_processes(print_process, NULL, 1);
    return 0;
  }

  // find pid for process name
  if (process_name[0] && !pid && !operate_on_kernel) {

    int num_found = pid_for_name(process_name, &pid, 0);
    if (num_found == 0) {
      printf("warning: no processes found by name; searching commands\n");
      ignore_match = getpid(); // don't match this process by command
      num_found = pid_for_name(process_name, &pid, 1);
    }
    if (num_found == 0) {
      printf("no processes found\n");
      return 0;
    }
    if (num_found > 1) {
      printf("multiple processes found\n");
      return 0;
    }
  }

  // pid missing?
  if (!pid && !operate_on_kernel) {
    printf("error: no process id or process name given\n");
    print_usage();
    return (-2);
  }

  // warn user if pid is memwatch itself
  if (pid == getpid()) {
    printf("warning: memwatch is operating on itself; -f is implied\n");
    freeze_while_operating = 0;
  }

  // warn user if not running as root
  if (geteuid())
    printf("warning: memwatch likely will not work if not run as root\n");

  // if a command is given on the cli, run it individually
  if (input_command)
    return run_one_command(pid, freeze_while_operating, max_results, input_command);

  // else, use the interactive interface
  return prompt_for_commands(pid, freeze_while_operating, max_results);
}
