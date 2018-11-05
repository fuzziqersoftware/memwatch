#pragma once

#include <sys/types.h>
#include <stdint.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "MemorySearch.hh"
#include "ProcessMemoryAdapter.hh"
#include "RegionFreezer.hh"


class MemwatchShell {
public:
  MemwatchShell(pid_t pid, uint64_t max_results, uint64_t max_search_iterations,
      bool pause_target = true, bool interactive = true, bool use_color = true);
  ~MemwatchShell() = default;

  int execute_commands();
  int execute_command(const std::string& args);

private:
  std::shared_ptr<ProcessMemoryAdapter> adapter;
  std::shared_ptr<RegionFreezer> freezer;

  pid_t pid;
  std::string process_name;

  bool pause_target;
  bool watch; // set to 1 to repeat commands
  bool interactive; // 0 if run from the shell; 1 if from the memwatch prompt
  bool run; // set to 0 to exit the memory search interface
  bool use_color;
  uint64_t num_commands_run;

  uint64_t max_results;
  uint64_t max_search_iterations;
  std::map<std::string, std::vector<MemorySearch>> name_to_searches;
  std::string current_search_name;

  std::vector<uint64_t> find_results;

  static const std::unordered_map<std::string, std::function<void(MemwatchShell&, const std::string&)>> command_to_executor;

  void dispatch_command(const std::string&);

  MemorySearch& get_latest_search(const std::string* name = NULL);
  std::vector<MemorySearch>& get_searches(const std::string* name = NULL);

  static std::vector<std::string> split_args(const std::string& args_str,
      size_t min_args = 0, size_t max_args = 0);
  static std::string read_typed_value(MemorySearch::Type type,
      const std::string& s);
  static void print_regions(FILE* stream,
      const std::vector<ProcessMemoryAdapter::Region>& regions);
  uint64_t get_addr_from_command(const std::string& args);
  void print_search_results(const MemorySearch& search);
  void print_thread_regs(int tid,
      const ProcessMemoryAdapter::ThreadState& state,
      const ProcessMemoryAdapter::ThreadState* prev);
  std::string details_for_iteration(const MemorySearch& s);

  void command_help(const std::string& args);
  void command_list(const std::string& args);
  void command_watch(const std::string& args);
  void command_dump(const std::string& args);
  void command_find(const std::string& args);
  void command_access(const std::string& args_str);
  void command_read(const std::string& args_str);
  void command_write(const std::string& args);
  void command_data(const std::string& args);
  void command_freeze(const std::string& args);
  void command_unfreeze(const std::string& args);
  void command_enable_disable(const std::string& args, bool enable);
  void command_enable(const std::string& args);
  void command_disable(const std::string& args);
  void command_frozen(const std::string& args);
  void command_open(const std::string& args_str);
  void command_fork(const std::string& args_str);
  void command_results(const std::string& args);
  void command_delete(const std::string& args_str);
  void command_search(const std::string& args_str);
  void command_iterations(const std::string& args_str);
  void command_truncate(const std::string& args_str);
  void command_undo(const std::string& args_str);
  void command_set(const std::string& args);
  void command_close(const std::string& args);
  void command_read_regs(const std::string& args_str);
  void command_write_regs(const std::string& args_str);
  void command_read_stacks(const std::string& args_str);
  void command_run(const std::string& args_str);
  void command_pause(const std::string& args_str);
  void command_resume(const std::string& args_str);
  void command_signal(const std::string& args_str);
  void command_attach(const std::string& args_str);
  void command_state(const std::string& args_str);
  void command_quit(const std::string& args_str);

  typedef void (MemwatchShell::*command_handler_t)(const std::string& args_str);
  static const std::unordered_map<std::string, command_handler_t> command_handlers;
};
