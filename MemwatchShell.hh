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
  MemwatchShell(pid_t pid, uint64_t max_results, bool pause_target = true,
      bool interactive = true, bool use_color = true);
  ~MemwatchShell() = default;

  int execute_commands();
  int execute_command(const std::string& args);

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
  std::map<std::string, MemorySearch> name_to_search;
  std::string current_search_name;

  std::vector<uint64_t> find_results;

  static const std::unordered_map<std::string, std::function<void(MemwatchShell&, const std::string&)>> command_to_executor;

  void dispatch_command(const std::string&);

  MemorySearch& get_search(const std::string* name = NULL);
};
