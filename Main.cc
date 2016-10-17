#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <phosg/Process.hh>

#include "MemwatchShell.hh"
#include "Signalable.hh"

using namespace std;


// signal handler for ctrl+c: cancel operations first; if there are none to
// cancel, then quit

uint64_t recent_cancel_time = 0;
uint64_t recent_cancel_count = 0;

void sigint_handler(int signum) {
  if (recent_cancel_time == time(NULL)) {
    if (recent_cancel_count > 1) {
      printf(" -- no operation to cancel - terminating\n");
      exit(0);
    } else {
      recent_cancel_count++;
    }
  } else {
    recent_cancel_time = time(NULL);
    recent_cancel_count = 0;
  }

  if (Signalable::signal_all()) {
    printf(" -- canceling operation\n");
  }
}

void print_usage() {
  printf("usage: memwatch [-c] [-p] [-nX] pid_or_name [command ...]\n");
  printf("see `man memwatch` for more information\n");
}

// entry point
int main(int argc, char* argv[]) {

  // hello there
  printf("fuzziqer software memwatch\n\n");

  // install our ctrl+c handler
  signal(SIGINT, sigint_handler);

  // only a few variables
  pid_t pid = 0;
  string process_name;
  bool use_color = true;
  bool pause_target = false;
  bool allow_parent = false;
  uint64_t max_results = 1024 * 1024 * 1024; // approx. 1 billion
  size_t num_bad_arguments = 0;
  string current_command;
  vector<string> commands;

  // parse command line args
  int x;
  for (x = 1; x < argc; x++) {

    if (argv[x][0] == '-') {

      // -c, --no-color: don't use colors in terminal output
      if (!strcmp(argv[x], "-c") || !strcmp(argv[x], "--no-color")) {
        use_color = false;

      // -f, --no-freeze: don't freeze target while operating on it
      } else if (!strcmp(argv[x], "-p") || !strcmp(argv[x], "--pause-target")) {
        pause_target = true;

      } else if (!strcmp(argv[x], "-P") || !strcmp(argv[x], "--allow-parent")) {
        allow_parent = true;

      // -n, --max-results: set maximum number of results
      } else if (!strncmp(argv[x], "-n", 2)) {
        max_results = strtoull(&argv[x][2], NULL, 0);
      } else if (!strncmp(argv[x], "--max-results=", 14)) {
        max_results = strtoull(&argv[x][14], NULL, 0);

      // complain if an unknown arg was given
      } else {
        printf("unknown command-line argument: %s\n", argv[x]);
        num_bad_arguments++;
      }

    // first non-dash param: a process name or pid
    } else if (!pid && !process_name[0]) {
      char* result;
      pid = strtoul(argv[x], &result, 0);
      if (*result) {
        process_name = argv[x];
      }

    // all subsequent non-dash params: command to be run
    } else {
      if (!strcmp(argv[x], ";")) {
        commands.emplace_back();
      } else {
        if (commands.empty()) {
          commands.emplace_back();
        }
        commands.back() += ' ';
        commands.back() += argv[x];
      }
    }
  }

  if (num_bad_arguments) {
    return 1;
  }

  // are we working on the kernel? (WOO DANGEROUS)
  bool operate_on_kernel = false;
  if (process_name == "KERNEL") {
    pid = 0;
    operate_on_kernel = true;
    if (pause_target) {
      printf("warning: operating on the kernel; -f may not be used\n");
      pause_target = false;
    }
  }

  // find pid for process name
  if (process_name[0] && !pid && !operate_on_kernel) {
    auto processes = list_processes(true);
    vector<pid_t> pids;
    pid_t self_pid = getpid(), parent_pid = allow_parent ? 0 : getppid();
    for (const auto& it : processes) {
      if ((it.first != self_pid) && (it.first != parent_pid) && strcasestr(it.second.c_str(), process_name.c_str())) {
        pids.emplace_back(it.first);
      }
    }
    sort(pids.begin(), pids.end());

    if (pids.size() == 0) {
      printf("error: no processes found\n");
      return 2;
    }

    if (pids.size() > 1) {
      printf("choose a process:\n");
      int index = 0;
      for (const auto& proc : pids) {
        printf("  (%d) %6d - %s\n", index, proc, processes[proc].c_str());
      }
      return 2;

    } else {
      pid = pids[0];
    }
  }

  // pid missing?
  if (!pid && !operate_on_kernel) {
    printf("error: no process id or process name given\n");
    print_usage();
    return 1;
  }

  // fail if pid is memwatch itself
  if (pid == getpid()) {
    printf("error: memwatch cannot operate on itself\n");
    return 2;
  }

  // warn user if not running as root
  if (geteuid()) {
    printf("warning: memwatch likely will not work if not run as root\n");
  }

  // if commands were given on the cli, run them individually
  if (!commands.empty()) {
    MemwatchShell sh(pid, max_results, pause_target, false, use_color);
    for (const string& command : commands) {
      int ret = sh.execute_command(command);
      if (ret) {
        return ret;
      }
    }
    return 0;
  }

  // else, use the interactive interface
  try {
    MemwatchShell sh(pid, max_results, pause_target, true, use_color);
    return sh.execute_commands();
  } catch (const exception& e) {
    fprintf(stderr, "error: %s\n", e.what());
    return 3;
  }
}
