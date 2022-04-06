#define _STDC_FORMAT_MACROS

#include "MemwatchShell.hh"

#include <assert.h>
#include <inttypes.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/syslimits.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <map>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Process.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Signalable.hh"

using namespace std;

#define HISTORY_FILE_LENGTH  1024



////////////////////////////////////////////////////////////////////////////////
// some utility functions

using Protection = ProcessMemoryAdapter::Region::Protection;

union PrimitiveValue {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int8_t s8;
  int16_t s16;
  int32_t s32;
  int64_t s64;
  float f;
  double d;
};

vector<string> MemwatchShell::split_args(const string& args_str,
    size_t min_args, size_t max_args) {

  vector<string> args = split(args_str, ' ');
  if (args.size() < min_args) {
    throw invalid_argument("not enough arguments");
  }
  if (max_args && (args.size() > max_args)) {
    throw invalid_argument("not enough arguments");
  }
  return args;
}

string MemwatchShell::read_typed_value(MemorySearch::Type type,
    const string& s) {
  string data_buffer(sizeof(PrimitiveValue), 0);
  PrimitiveValue* value = (PrimitiveValue*)data_buffer.data();
  if (MemorySearch::is_integer_search_type(type)) {
    value->u64 = stoull(s, NULL, 0);
  } else if (type == MemorySearch::Type::FLOAT || type == MemorySearch::Type::FLOAT_RE) {
    value->f = stof(s);
  } else if (type == MemorySearch::Type::DOUBLE || type == MemorySearch::Type::DOUBLE_RE) {
    value->d = stod(s);
  } else if (type == MemorySearch::Type::DATA) {
    data_buffer = parse_data_string(s);
  }

  // byteswap the value if the search is a reverse-endian type
  size_t size = MemorySearch::value_size_for_search_type(type);
  if (MemorySearch::is_reverse_endian_search_type(type)) {
    if (size == 2) {
      value->u16 = bswap16(value->u16);
    } else if (size == 4) {
      value->u32 = bswap32(value->u32);
    } else if (size == 8) {
      value->u64 = bswap64(value->u64);
    }
  }

  if (type != MemorySearch::Type::DATA) {
    data_buffer.resize(size);
  }

  return data_buffer;
}

void MemwatchShell::print_regions(FILE* stream,
    const vector<ProcessMemoryAdapter::Region>& regions) {

  // initialize the counters
  size_t total_error = 0, total_readable = 0, total_writable = 0,
      total_executable = 0, total_accessible = 0, total_inaccessible = 0,
      total_bytes = 0;
  size_t num_error = 0, num_readable = 0, num_writable = 0,
      num_executable = 0, num_accessible = 0, num_inaccessible = 0;

  fprintf(stream, "# addr size access/max_access\n");
  for (const auto& region : regions) {
    fprintf(stream, "%016" PRIX64 " %016zX %c%c%c/%c%c%c%s\n",
        region.addr,
        region.size,
        (region.protection & Protection::READABLE) ? 'r' : '-',
        (region.protection & Protection::WRITABLE) ? 'w' : '-',
        (region.protection & Protection::EXECUTABLE) ? 'x' : '-',
        (region.max_protection & Protection::READABLE) ? 'r' : '-',
        (region.max_protection & Protection::READABLE) ? 'w' : '-',
        (region.max_protection & Protection::READABLE) ? 'x' : '-',
        region.data.empty() ? " [data not read]" : "");

    // increment the relevant counters
    if (region.data.empty()) {
      total_error += region.size;
      num_error++;
    }
    if (region.protection & Protection::READABLE) {
      total_readable += region.size;
      num_readable++;
    }
    if (region.protection & Protection::WRITABLE) {
      total_writable += region.size;
      num_writable++;
    }
    if (region.protection & Protection::EXECUTABLE) {
      total_executable += region.size;
      num_executable++;
    }
    if (region.protection & Protection::ALL_ACCESS) {
      total_accessible += region.size;
      num_accessible++;
    } else {
      total_inaccessible += region.size;
      num_inaccessible++;
    }
    total_bytes += region.size;
  }

  // print the counters
  string total_bytes_str = format_size(total_bytes);
  string total_error_str = format_size(total_error);
  string total_accessible_str = format_size(total_accessible);
  string total_readable_str = format_size(total_readable);
  string total_writable_str = format_size(total_writable);
  string total_executable_str = format_size(total_executable);
  string total_inaccessible_str = format_size(total_inaccessible);

  fprintf(stream, "# %5zu regions, %s in total\n", regions.size(), total_bytes_str.c_str());
  fprintf(stream, "# %5zu regions, %s unread by memwatch\n", num_error, total_error_str.c_str());
  fprintf(stream, "# %5zu regions, %s accessible\n", num_accessible, total_accessible_str.c_str());
  fprintf(stream, "# %5zu regions, %s readable\n", num_readable, total_readable_str.c_str());
  fprintf(stream, "# %5zu regions, %s writable\n", num_writable, total_writable_str.c_str());
  fprintf(stream, "# %5zu regions, %s executable\n", num_executable, total_executable_str.c_str());
  fprintf(stream, "# %5zu regions, %s inaccessible\n", num_inaccessible, total_inaccessible_str.c_str());
}

uint64_t MemwatchShell::get_addr_from_command(const string& args) {
  if (args[0] == 'x' || args[0] == 's') {
    size_t num_end_pos = 0;
    uint64_t index = stoull(args.substr(1), &num_end_pos, 0);
    string search_name;
    if (num_end_pos + 1 < args.size()) {
      if (args[num_end_pos + 1] == '@') {
        search_name = args.substr(num_end_pos + 2);
      } else if (args[num_end_pos + 1] != ' ') {
        throw invalid_argument("extra characters after result id");
      }
    }

    const auto& s = this->get_latest_search(search_name.empty() ? NULL : &search_name);

    if (index >= s.get_results().size()) {
      throw invalid_argument("search does not have enough results");
    }
    return s.get_results()[index];

  } else if (args[0] == 't') {
    size_t num_end_pos = 0;
    uint64_t index = stoull(args.substr(1), &num_end_pos, 0);
    if ((num_end_pos + 1 < args.size()) && (args[num_end_pos + 1] != ' ')) {
      throw invalid_argument("extra characters after result id");
    }
    if (index >= this->find_results.size()) {
      throw invalid_argument("find does not have enough results");
    }
    return this->find_results[index];
  }

  return stoull(args, NULL, 16);
}


////////////////////////////////////////////////////////////////////////////////
// command handlers

// list (no arguments)
void MemwatchShell::command_list(const string&) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    regions = this->adapter->get_all_regions();
  }
  this->print_regions(stdout, regions);
}

// watch <command> [args...]
void MemwatchShell::command_watch(const string& args) {
  this->watch = 1;
  this->execute_command(args);
  this->watch = 0;
}

// dump [filename_prefix]
void MemwatchShell::command_dump(const string& args) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    regions = this->adapter->get_all_regions(true);
  }

  if (!args.empty()) {
    {
      auto f = fopen_unique(string_printf("%s.index.txt", args.c_str()), "wt");
      print_regions(f.get(), regions);
    }

    for (const auto& region : regions) {
      if (!region.data.empty()) {
        string filename = string_printf("%s.%016" PRIX64 ".%016" PRIX64 ".bin",
            args.c_str(), region.addr, region.addr + region.size);
        save_file(filename, region.data);
      }
    }

  } else {
    print_regions(stdout, regions);
  }
}

// find <data>
void MemwatchShell::command_find(const string& args) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    regions = this->adapter->get_all_regions(true);
  }

  string mask;
  string data = parse_data_string(args, &mask);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  this->find_results.clear();

  Signalable s;
  for (const auto& region : regions) {
    if (s.is_signaled()) {
      break;
    }
    if (region.data.empty()) {
      continue;
    }

    // search through this region's data for the string
    size_t y, z;
    for (y = 0; y <= region.size - data.size(); y++) {
      for (z = 0; z < data.size(); z++) {
        if (mask[z] && (region.data[y + z] != data[z])) {
          break;
        }
      }

      if (z == data.size()) {
        printf("(%zu) %016" PRIX64 " (%c%c%c)\n", this->find_results.size(),
            region.addr + y,
            (region.protection & Protection::READABLE) ? 'r' : '-',
            (region.protection & Protection::WRITABLE) ? 'w' : '-',
            (region.protection & Protection::EXECUTABLE) ? 'x' : '-');
        this->find_results.emplace_back(region.addr + y);
      }
    }
  }
}

// access <addr> [mode]
void MemwatchShell::command_access(const string& args_str) {

  auto args = this->split_args(args_str, 1, 2);

  uint8_t prot = Protection::ALL_ACCESS;
  if (args.size() > 1) {
    prot = 0;
    if (strchr(args[1].c_str(), 'r')) {
      prot |= Protection::READABLE;
    }
    if (strchr(args[1].c_str(), 'w')) {
      prot |= Protection::WRITABLE;
    }
    if (strchr(args[1].c_str(), 'x')) {
      prot |= Protection::EXECUTABLE;
    }
  }

  uint64_t addr = this->get_addr_from_command(args[0]);
  this->adapter->set_protection(addr, 0, prot, Protection::ALL_ACCESS);
}

// read <addr> <size> [+format]
void MemwatchShell::command_read(const string& args_str) {

  auto args = this->split_args(args_str, 2, 4);
  uint64_t addr = this->get_addr_from_command(args[0]);
  uint64_t size = stoull(args[1], NULL, 16);

  const string* filename = NULL;
  uint64_t print_flags = PrintDataFlags::PRINT_ASCII | PrintDataFlags::USE_COLOR;
  for (size_t x = 2; x < args.size(); x++) {
    if (args[x].empty()) {
      continue;
    }
    if (args[x][0] == '+') {
      print_flags = 0;
      for (char ch : args[x]) {
        if (ch == '+') {
          continue;
        } else if (ch == 'a') {
          print_flags |= PrintDataFlags::PRINT_ASCII;
        } else if (ch == 'f') {
          print_flags |= PrintDataFlags::PRINT_FLOAT;
        } else if (ch == 'd') {
          print_flags |= PrintDataFlags::PRINT_DOUBLE;
        } else if (ch == 'r') {
          print_flags |= PrintDataFlags::REVERSE_ENDIAN;
        }
      }
    } else {
      filename = &args[x];
    }
  }

  // allocate a local read buffer, and a buffer for the previous data if we're
  // repeating the read
  string data(size, 0);
  unique_ptr<string> prev_data(this->watch ? new string(size, 0) : NULL);

  // go ahead and read the target's memory
  bool first_read = true;
  Signalable s;
  do {
    string time_str = format_time();
    try {
      {
        PauseGuard g(this->pause_target ? this->adapter : NULL);
        data = this->adapter->read(addr, size);
      }

      if (filename) {
        save_file(*filename, data);
        printf("%s @ %016" PRIX64 ":%016" PRIX64 " // %s > %s\n",
            this->process_name.c_str(), addr, size, time_str.c_str(),
            filename->c_str());
      } else {
        printf("%s @ %016" PRIX64 ":%016" PRIX64 " // %s\n",
            this->process_name.c_str(), addr, size, time_str.c_str());
        print_data(stdout, data.data(), data.size(), addr,
            first_read ? NULL : prev_data->data(), print_flags);
        printf("\n");
      }

    } catch (const exception& e) {
      printf("%s @ %016" PRIX64 ":%016" PRIX64 " // %s // failed [%s]\n",
          this->process_name.c_str(), addr, size, time_str.c_str(), e.what());
    }

    if (this->watch) {
      usleep(1000000); // wait a second before repeating the read
    }

    if (prev_data.get()) {
      prev_data->swap(data);
    }
    first_read = false;

  } while (this->watch && !s.is_signaled());
}

// write <addr> <data>
void MemwatchShell::command_write(const string& args) {
  uint64_t addr = this->get_addr_from_command(args);

  size_t offset = skip_whitespace(args, skip_non_whitespace(args, 0));
  string data = parse_data_string(args.substr(offset));
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  try {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    this->adapter->write(addr, data);
    printf("wrote %zu (0x%zX) bytes\n", data.size(), data.size());

  } catch (const exception& e) {
    printf("failed to write data to process (%s)\n", e.what());
  }
}

// data <data>
void MemwatchShell::command_data(const string& args) {
  string mask;
  string data = parse_data_string(args, &mask);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  printf("read %zu (0x%zX) data bytes:\n", data.size(), data.size());
  print_data(stdout, data.data(), data.size());
  printf("read %zu (0x%zX) mask bytes:\n", mask.size(), mask.size());
  print_data(stdout, mask.data(), mask.size());
}

// freeze [+n<name>] <address-or-result-id> <data|+s<size>>
// freeze [+n<name>] <address-or-result-id> +m<max-entries> <data|+s<size>> [+N null-data]
void MemwatchShell::command_freeze(const string& args) {

  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  string name;
  string addr_arg;
  uint64_t addr = 0;
  uint64_t array_max_entries = 0;
  uint64_t read_size = 0;
  string data_to_parse;
  string null_data_to_parse;
  bool reading_null_data_to_parse = false;
  bool enable = true;
  for (string arg : this->split_args(args, 2, 0)) {
    if (arg[0] == '+') {
      switch (arg[1]) {
        case 'n':
          name = arg.substr(2);
          break;
        case 's':
          read_size = stoull(arg.substr(2), NULL, 16);
          break;
        case 'm':
          array_max_entries = stoull(arg.substr(2));
          break;
        case 'd':
          enable = false;
          break;
        case 'N':
          reading_null_data_to_parse = true;
          if (arg.size() > 2) {
            null_data_to_parse += arg.substr(2);
          }
          break;
        default:
          throw invalid_argument("unknown option: " + arg);
      }

    } else {
      if (addr) {
        if (reading_null_data_to_parse) {
          null_data_to_parse += arg;
        } else {
          data_to_parse += arg;
        }
      } else {
        addr_arg = arg;
        addr = this->get_addr_from_command(addr_arg);
      }
    }
  }

  // generate a name if none was given
  if (name.empty()) {
    try {
      this->get_latest_search();
      // if there's an override search name, use that instead of the current
      // search name
      size_t at_pos = addr_arg.find('@');
      if (at_pos != string::npos) {
        name = addr_arg.substr(at_pos + 1);
      } else {
        name = this->current_search_name;
      }
    } catch (const out_of_range& e) {
      name = "";
    } catch (const invalid_argument& e) {
      name = "";
    }
  }

  // read the data or parse it
  string data;
  string data_mask;
  string null_data;
  string null_data_mask;
  if (read_size) {
    data.clear();
    data.resize(read_size);
    data_mask.clear();
    data_mask.resize(read_size, 0xFF);

    {
      PauseGuard g(this->pause_target ? this->adapter : NULL);
      data = this->adapter->read(addr, read_size);
    }
  } else {
    data = parse_data_string(data_to_parse, &data_mask);
    null_data = parse_data_string(null_data_to_parse, &null_data_mask);
  }

  // make sure the null args make sense
  if (data.empty()) {
    throw invalid_argument("no data was given");
  }
  if (!null_data.empty()) {
    if (!array_max_entries) {
      throw invalid_argument("+m must be used if null data is given");
    }
    if (null_data.size() != data.size()) {
      throw invalid_argument("data and null data must have the same size");
    }
  }

  // add it to the frozen-list
  if (array_max_entries) {
    if (null_data.empty()) {
      this->freezer->freeze_array(name, addr, array_max_entries, data,
          data_mask, NULL, NULL, enable);
    } else {
      this->freezer->freeze_array(name, addr, array_max_entries, data,
          data_mask, &null_data, &null_data_mask, enable);
    }
  } else {
    this->freezer->freeze(name, addr, data, enable);
  }
}

// unfreeze <<name>|<index>|<addr>>
void MemwatchShell::command_unfreeze(const string& args) {

  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args == "*") {
    size_t num_regions = this->freezer->unfreeze_all();
    printf("%zu regions unfrozen\n", num_regions);

  } else if (!args.empty()) {

    // first try to unfreeze by name
    size_t num_regions = this->freezer->unfreeze_name(args);
    if (num_regions == 1) {
      printf("region unfrozen\n");
      return;
    }
    if (num_regions > 1) {
      printf("%zu regions unfrozen\n", num_regions);
      return;
    }

    // if unfreezing by name didn't match any regions, unfreeze by address
    uint64_t addr = this->get_addr_from_command(args);
    if (this->freezer->unfreeze_addr(addr)) {
      printf("region unfrozen\n");
      return;
    }

    // if that didn't work either, try to unfreeze by index
    size_t offset = 0;
    uint64_t index = stoull(args, &offset);
    if (offset == 0) {
      throw invalid_argument("bad argument to unfreeze");
    }
    if (this->freezer->unfreeze_index(index)) {
      printf("region unfrozen\n");
    } else {
      throw out_of_range("no regions matched");
    }

  // else, print frozen regions
  } else {
    this->freezer->print_regions(stdout);
  }
}

// <enable|disable> <<name>|<index>|<addr>>
void MemwatchShell::command_enable_disable(const string& args,
    bool enable) {

  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args == "*") {
    size_t num_regions = this->freezer->enable_all(enable);
    printf("%zu regions %s\n", num_regions, enable ? "enabled" : "disabled");

  } else if (!args.empty()) {

    // first try to enable/disable by name
    size_t num_regions = this->freezer->enable_name(args, enable);
    if (num_regions == 1) {
      printf("region %s\n", enable ? "enabled" : "disabled");
      return;
    }
    if (num_regions > 1) {
      printf("%zu regions %s\n", num_regions, enable ? "enabled" : "disabled");
      return;
    }

    // if unfreezing by name didn't match any regions, enable/disable by address
    uint64_t addr = this->get_addr_from_command(args);
    if (this->freezer->enable_addr(addr, enable)) {
      printf("region %s\n", enable ? "enabled" : "disabled");
      return;
    }

    // if that didn't work either, try to enable/disable by index
    size_t offset = 0;
    uint64_t index = stoull(args, &offset);
    if (offset == 0) {
      throw invalid_argument("bad argument to enable/disable");
    }
    if (this->freezer->enable_index(index, enable)) {
      printf("region %s\n", enable ? "enabled" : "disabled");
    } else {
      throw out_of_range("no regions matched");
    }

  // else, print frozen regions
  } else {
    this->freezer->print_regions(stdout);
  }
}

// enable <<name>|<index>|<addr>>
void MemwatchShell::command_enable(const string& args) {
  this->command_enable_disable(args, true);
}

// disable <<name>|<index>|<addr>>
void MemwatchShell::command_disable(const string& args) {
  this->command_enable_disable(args, false);
}

// frozen [data|commands|cmds]
void MemwatchShell::command_frozen(const string& args) {

  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args == "data") {
    this->freezer->print_regions(stdout, true);
  } else if ((args == "cmds") || (args == "commands")) {
    this->freezer->print_regions_commands(stdout);
  } else {
    this->freezer->print_regions(stdout);
  }
}

// open
// open <name>
// open <type> [name]
// open <type> name .
// open <type> name operator value
void MemwatchShell::command_open(const string& args_str) {

  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  vector<string> args = this->split_args(args_str, 0, 4);

  if (args.size() == 0) {
    for (const auto& map_it : this->name_to_searches) {
      auto& searches = map_it.second;

      string line;
      if (searches.empty()) {
        line = string_printf("(unknown type) %s\n", map_it.first.c_str());
      } else {
        line = string_printf("%s %s with iterations [",
            MemorySearch::name_for_search_type(searches[0].get_type()),
            map_it.first.c_str());

        for (size_t x = 0; x < searches.size(); x++) {
          if (x > 0) {
            line += ", (";
          } else {
            line += '(';
          }

          const MemorySearch& search = searches[x];
          line += this->details_for_iteration(search);

          line += ')';
        }
        line += ']';
      }

      printf("  %s\n", line.c_str());
    }
    printf("total searches: %zu\n", this->name_to_searches.size());
    return;
  }

  // if the first argument is the name of a search, switch to that search
  if (this->name_to_searches.count(args[0]) && (args.size() == 1)) {
    this->current_search_name = args[0];
    if (!this->current_search_name.empty()) {
      if (this->name_to_searches.erase("")) {
        printf("closed unnamed search\n");
      }
    }
    printf("switched to search %s\n", this->current_search_name.c_str());
    return;
  }

  // if the first argument is a valid Type, then open a new search
  MemorySearch::Type type;
  try {
    type = MemorySearch::search_type_for_name(args[0].c_str());
  } catch (const invalid_argument& e) {
    throw invalid_argument("no search named " + args[0]);
  }

  string name = (args.size() >= 2) ? args[1] : "";
  bool all_memory = (args[0].find('!') != string::npos);

  this->name_to_searches.erase(name);

  vector<MemorySearch> searches;
  searches.emplace_back(type, all_memory);
  this->name_to_searches.emplace(name, move(searches));

  this->current_search_name = name;
  if (!this->current_search_name.empty()) {
    if (this->name_to_searches.erase("")) {
      printf("closed unnamed search\n");
    }
  }

  if (this->current_search_name.empty()) {
    printf("opened new %s search (unnamed)\n", MemorySearch::name_for_search_type(type));
  } else {
    printf("opened new %s search named %s\n", MemorySearch::name_for_search_type(type), name.c_str());
  }

  // if more arguments were given, perform the initial search
  if (args.size() > 2) {
    if (args.size() == 3) {
      this->command_search(args[2]);
    } else {
      this->command_search(args[2] + " " + args[3]);
    }
  }
}

// fork <name>
// fork <name1> <name2>
void MemwatchShell::command_fork(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = this->split_args(args_str, 1, 2);
  if (args.size() == 1) {
    // fork the current search into a new one and switch to it
    if (args[0] == this->current_search_name) {
      throw invalid_argument("can\'t fork a search into itself");
    }
    const vector<MemorySearch>& current_searches = this->get_searches();

    this->name_to_searches.erase(args[0]);
    this->name_to_searches.emplace(args[0], current_searches);
    printf("forked search %s into %s\n", this->current_search_name.c_str(),
        args[0].c_str());
    this->current_search_name = args[0];

  } else {
    try {
      this->name_to_searches.erase(args[1]);
      this->name_to_searches.emplace(args[1], this->name_to_searches.at(args[0]));
      printf("forked search %s into %s\n", args[0].c_str(), args[1].c_str());
    } catch (const out_of_range& e) {
      throw out_of_range("no search named " + args[0]);
    }
  }
}

void MemwatchShell::print_search_results(const MemorySearch& search) {

  uint64_t x = 0;
  if (search.get_type() == MemorySearch::Type::DATA) {
    for (uint64_t result : search.get_results()) {
      printf("(%" PRIu64 ") %016" PRIX64 "\n", x, result);
      x++;
    }

  } else {
    size_t size = MemorySearch::value_size_for_search_type(search.get_type());

    assert(size <= 8);

    for (uint64_t result : search.get_results()) {
      printf("(%" PRIu64 ") %016" PRIX64 " ", x, result);
      try {
        string data = this->adapter->read(result, size);
        PrimitiveValue* value = (PrimitiveValue*)data.data();

        switch (search.get_type()) {
          case MemorySearch::Type::UINT8:
            printf("%" PRIu8 " (0x%02" PRIX8 ")\n", value->u8, value->u8);
            break;
          case MemorySearch::Type::UINT16_RE:
            value->u16 = bswap16(value->u16);
          case MemorySearch::Type::UINT16:
            printf("%" PRIu16 " (0x%04" PRIX16 ")\n", value->u16, value->u16);
            break;
          case MemorySearch::Type::UINT32_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::UINT32:
            printf("%" PRIu32 " (0x%08" PRIX32 ")\n", value->u32, value->u32);
            break;
          case MemorySearch::Type::UINT64_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::UINT64:
            printf("%" PRIu64 " (0x%016" PRIX64 ")\n", value->u64, value->u64);
            break;

          case MemorySearch::Type::INT8:
            printf("%" PRId8 " (0x%02" PRIX8 ")\n", value->s8, value->s8);
            break;
          case MemorySearch::Type::INT16_RE:
            value->u16 = bswap16(value->u16);
          case MemorySearch::Type::INT16:
            printf("%" PRId16 " (0x%04" PRIX16 ")\n", value->s16, value->s16);
            break;
          case MemorySearch::Type::INT32_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::INT32:
            printf("%" PRId32 " (0x%08" PRIX32 ")\n", value->s32, value->s32);
            break;
          case MemorySearch::Type::INT64_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::INT64:
            printf("%" PRId64 " (0x%016" PRIX64 ")\n", value->s64, value->s64);
            break;

          case MemorySearch::Type::FLOAT_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::FLOAT:
            printf("%f (0x%08" PRIX32 ")\n", value->f, value->u32);
            break;
          case MemorySearch::Type::DOUBLE_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::DOUBLE:
            printf("%lf (0x%016" PRIX64 ")\n", value->d, value->u64);
            break;

          case MemorySearch::Type::DATA:
            assert(false); // this should never happen; we handled DATA above
        }

      } catch (const exception& e) {
        printf("<< memory not readable, %s >>\n", e.what());
      }

      x++;
    }
  }
}

// results [search_name]
void MemwatchShell::command_results(const string& args) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  MemorySearch& search = this->get_latest_search(args.empty() ? NULL : &args);

  if (!search.has_memory_snapshot()) {
    throw invalid_argument("search not found, or no search currently open");
  }

  Signalable s;
  do {
    this->print_search_results(search);
    if (this->watch) {
      usleep(1000000); // wait a second, if we're repeating
    }
  } while (this->watch && !s.is_signaled());
}

// delete <spec> [spec ...]
void MemwatchShell::command_delete(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  MemorySearch& search = this->get_latest_search();

  auto args = this->split_args(args_str, 1);

  // read the addresses
  vector<pair<uint64_t, uint64_t>> to_delete;
  to_delete.reserve(args.size());
  for (const auto& arg : args) {
    size_t dash_pos = arg.find('-');
    uint64_t addr1, addr2;
    if (dash_pos != string::npos) {
      addr1 = this->get_addr_from_command(arg.substr(0, dash_pos));
      addr2 = this->get_addr_from_command(arg.substr(dash_pos + 1));
    } else {
      addr1 = addr2 = this->get_addr_from_command(arg);
    }

    if (addr1 > addr2) {
      throw invalid_argument("range is specified in decreasing order");
    }
    to_delete.push_back(make_pair(addr1, addr2));
  }

  // delete the search results
  for (const auto& addr_pair : to_delete) {
    search.delete_results(addr_pair.first, addr_pair.second);
  }

  // print the remaining results if there aren't too many
  if (search.get_results().size() <= 20) {
    this->print_search_results(search);
  }
}

// search [name] <predicate> [value]
void MemwatchShell::command_search(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = this->split_args(args_str, 1, 3);

  string name = this->current_search_name;
  MemorySearch::Predicate predicate;
  string value;
  string annotation;
  try {
    predicate = MemorySearch::search_predicate_for_name(args[0].c_str());
    if (args.size() > 1) {
      value = args[1];
      annotation = args[0] + " " + args[1] + " at ";
    } else {
      annotation = args[0] + " previous at ";
    }

  } catch (const out_of_range& e) {
    if (args.size() < 2) {
      throw invalid_argument("not enough arguments");
    }
    name = args[0];
    predicate = MemorySearch::search_predicate_for_name(args[1].c_str());
    if (args.size() > 2) {
      value = args[2];
      annotation = args[1] + " " + args[2] + " at ";
    } else {
      annotation = args[1] + " previous at ";
    }
  }

  annotation += format_time(now());

  vector<MemorySearch>& searches = this->get_searches(&name);
  if (searches.empty()) {
    throw runtime_error("search has no iterations");
  }
  MemorySearch& latest_search = searches.back();

  // convert the value into something we can search for
  if (!value.empty()) {
    value = this->read_typed_value(latest_search.get_type(), value);
  }

  latest_search.check_can_update(predicate, value);

  shared_ptr<vector<ProcessMemoryAdapter::Region>> regions(new vector<ProcessMemoryAdapter::Region>());
  {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    if (latest_search.has_valid_results()) {
      *regions = this->adapter->get_target_regions(latest_search.get_results(), true);
    } else {
      *regions = this->adapter->get_all_regions(true);
    }
  }

  MemorySearch new_search = latest_search;
  new_search.set_annotation(annotation);
  new_search.update(regions, predicate, value, this->max_results, stderr);
  if (new_search.get_results().size() <= 20) {
    this->print_search_results(new_search);
  }

  searches.emplace_back(move(new_search));

  ssize_t num_to_delete = searches.size() - this->max_search_iterations;
  if (num_to_delete > 0) {
    searches.erase(searches.begin(), searches.begin() + num_to_delete);
  }
}

// iterations [name]
void MemwatchShell::command_iterations(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  for (const auto& iteration : this->get_searches(args_str.empty() ? NULL : &args_str)) {
    string s = this->details_for_iteration(iteration);
    printf("  %s\n", s.c_str());
  }
}

// truncate [name] <count-to-retain>
void MemwatchShell::command_truncate(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = this->split_args(args_str, 1, 2);

  vector<MemorySearch>* searches = NULL;
  ssize_t count_to_retain = 0;
  if (args.size() == 2) {
    searches = &this->get_searches(&args[0]);
    count_to_retain = stoll(args[1]);
  } else {
    searches = &this->get_searches();
    count_to_retain = stoll(args[0]);
  }

  if (count_to_retain <= 0) {
    throw invalid_argument("retained iteration count must be positive");
  }

  // count_to_retain is a positive number, so we can't possibly delete all items
  // in searches here
  ssize_t num_to_delete = searches->size() - count_to_retain;
  if (num_to_delete > 0) {
    searches->erase(searches->begin(), searches->begin() + num_to_delete);
    printf("deleted %zd iterations\n", num_to_delete);
  } else {
    printf("deleted 0 iterations\n");
  }
}

// undo [name]
void MemwatchShell::command_undo(const string& args_str) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  vector<MemorySearch>& searches = this->get_searches(args_str.empty() ? NULL : &args_str);
  if (searches.size() <= 1) {
    throw invalid_argument("cannot undo initial search");
  }
  searches.pop_back();
}

// set <value>
// set s<result-index> <value>
void MemwatchShell::command_set(const string& args) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  string value_arg;
  int64_t result_index = -1;
  if (args[0] == 's') {
    size_t space_pos = args.find(' ');
    if (space_pos == string::npos) {
      throw invalid_argument("no data was specified");
    }
    result_index = stoll(args.substr(1, space_pos - 1), NULL, 0);
    value_arg = args.substr(space_pos + 1);
  } else {
    value_arg = args;
  }

  MemorySearch& search = this->get_latest_search();
  if (!search.has_memory_snapshot()) {
    throw invalid_argument("no initial search performed");
  }

  string data = this->read_typed_value(search.get_type(), value_arg);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  {
    PauseGuard g(this->pause_target ? this->adapter : NULL);
    if (result_index < 0) {
      for (uint64_t result : search.get_results()) {
        try {
          this->adapter->write(result, data);
          printf("%016" PRIX64 ": wrote %zu (0x%zX) bytes\n", result, data.size(),
              data.size());
        } catch (const exception& e) {
          printf("%016" PRIX64 ": failed to write data (%s)\n", result, e.what());
        }
      }
    } else {
      uint64_t result = search.get_results().at(result_index);
      try {
        this->adapter->write(result, data);
        printf("%016" PRIX64 ": wrote %zu (0x%zX) bytes\n", result, data.size(),
            data.size());
      } catch (const exception& e) {
        printf("%016" PRIX64 ": failed to write data (%s)\n", result, e.what());
      }
    }
  }
}

// close [name]
void MemwatchShell::command_close(const string& args) {
  if (!this->interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args.empty()) {
    if (!this->name_to_searches.erase(this->current_search_name)) {
      throw invalid_argument("no search currently open");
    }
    this->current_search_name.clear();

  } else {
    if (!this->name_to_searches.erase(args)) {
      throw invalid_argument("no open search named " + args);
    }
    if (this->current_search_name == args) {
      this->current_search_name.clear();
    }
  }
}

// pause
void MemwatchShell::command_pause(const string&) {
  this->adapter->pause();
}

// resume
void MemwatchShell::command_resume(const string&) {
  this->adapter->resume();
}

// signal <signum>
void MemwatchShell::command_signal(const string& args_str) {
  int signum = stoi(args_str.c_str());
  if (kill(this->pid, signum)) {
    throw runtime_error(string_printf("failed to send signal %d to process", signum));
  }
}

// attach [name_or_pid]
void MemwatchShell::command_attach(const string& args_str) {

  // if no command given, use the current process name
  // (i.e. if the program was quit & restarted, its pid will change)
  const string& name = args_str.empty() ? this->process_name : args_str;

  // try to read new pid
  pid_t new_pid = 0;
  try {
    new_pid = stoi(name);
  } catch (const exception& e) { }
  if (!new_pid) {
    new_pid = pid_for_name(name);
  }

  // check if the name can be found
  string new_name = name_for_pid(new_pid);

  // if we get here, then the process exists - attach to it
  this->pid = new_pid;
  this->process_name = new_name;
  this->adapter->attach(this->pid);
}

// state [<field> <value>]
void MemwatchShell::command_state(const string& args_str) {

  if (args_str.empty()) {
    printf("use_color = %d\n", this->use_color);
    printf("pid = %d\n", this->pid);
    printf("process_name = \"%s\"\n", this->process_name.c_str());
    printf("pause_target = %d\n", this->pause_target);
    printf("max_results = %" PRIu64 "\n", this->max_results);
    printf("max_search_iterations = %" PRIu64 "\n", this->max_search_iterations);
    printf("interactive = %d\n", this->interactive);
    printf("run = %d\n", this->run);
    printf("num_commands_run = %" PRIu64 "\n", this->num_commands_run);
    printf("num_find_results = %zu\n", this->find_results.size());
    return;
  }

  auto args = this->split_args(args_str, 2, 2);

  if (args[0] == "use_color") {
    this->use_color = stoi(args[1]);
  } else if (args[0] == "process_name") {
    this->process_name = args[1];
  } else if (args[0] == "pause_target") {
    this->pause_target = stoi(args[1]);
  } else if (args[0] == "max_results") {
    this->max_results = stoull(args[1]);
  } else if (args[0] == "max_search_iterations") {
    this->max_search_iterations = stoull(args[1]);
  } else if (args[0] == "run") {
    this->run = stoi(args[1]);
  } else {
    throw invalid_argument("unknown or read-only field");
  }
}

// quit
void MemwatchShell::command_quit(const string&) {
  this->run = false;
}



const unordered_map<string, MemwatchShell::command_handler_t> MemwatchShell::command_handlers({
  {"access",     &MemwatchShell::command_access},
  {"acc",        &MemwatchShell::command_access},
  {"a",          &MemwatchShell::command_access},
  {"attach",     &MemwatchShell::command_attach},
  {"att",        &MemwatchShell::command_attach},
  {"at",         &MemwatchShell::command_attach},
  {"close",      &MemwatchShell::command_close},
  {"cls",        &MemwatchShell::command_close},
  {"c",          &MemwatchShell::command_close},
  {"cz",         &MemwatchShell::command_undo},
  {"data",       &MemwatchShell::command_data},
  {"delete",     &MemwatchShell::command_delete},
  {"del",        &MemwatchShell::command_delete},
  {"disable",    &MemwatchShell::command_disable},
  {"dis",        &MemwatchShell::command_disable},
  {"di",         &MemwatchShell::command_disable},
  {"dt",         &MemwatchShell::command_data},
  {"d",          &MemwatchShell::command_disable},
  {"dump",       &MemwatchShell::command_dump},
  {"dmp",        &MemwatchShell::command_dump},
  {"enable",     &MemwatchShell::command_enable},
  {"ena",        &MemwatchShell::command_enable},
  {"en",         &MemwatchShell::command_enable},
  {"exit",       &MemwatchShell::command_quit},
  {"e",          &MemwatchShell::command_enable},
  {"find",       &MemwatchShell::command_find},
  {"fi",         &MemwatchShell::command_find},
  {"fork",       &MemwatchShell::command_fork},
  {"fk",         &MemwatchShell::command_fork},
  {"fo",         &MemwatchShell::command_fork},
  {"frozen",     &MemwatchShell::command_frozen},
  {"fzn",        &MemwatchShell::command_frozen},
  {"freeze",     &MemwatchShell::command_freeze},
  {"fr",         &MemwatchShell::command_freeze},
  {"f",          &MemwatchShell::command_freeze},
  {"iterations", &MemwatchShell::command_iterations},
  {"iters",      &MemwatchShell::command_iterations},
  {"its",        &MemwatchShell::command_iterations},
  {"it",         &MemwatchShell::command_iterations},
  {"i",          &MemwatchShell::command_iterations},
  {"list",       &MemwatchShell::command_list},
  {"ls",         &MemwatchShell::command_list},
  {"l",          &MemwatchShell::command_list},
  {"open",       &MemwatchShell::command_open},
  {"o",          &MemwatchShell::command_open},
  {"pause",      &MemwatchShell::command_pause},
  {"quit",       &MemwatchShell::command_quit},
  {"q",          &MemwatchShell::command_quit},
  {"read",       &MemwatchShell::command_read},
  {"rd",         &MemwatchShell::command_read},
  {"results",    &MemwatchShell::command_results},
  {"resume",     &MemwatchShell::command_resume},
  {"res",        &MemwatchShell::command_results},
  {"r",          &MemwatchShell::command_read},
  {"search",     &MemwatchShell::command_search},
  {"s",          &MemwatchShell::command_search},
  {"set",        &MemwatchShell::command_set},
  {"signal",     &MemwatchShell::command_signal},
  {"sig",        &MemwatchShell::command_signal},
  {"state",      &MemwatchShell::command_state},
  {"st",         &MemwatchShell::command_state},
  {"truncate",   &MemwatchShell::command_truncate},
  {"trunc",      &MemwatchShell::command_truncate},
  {"tr",         &MemwatchShell::command_truncate},
  {"t",          &MemwatchShell::command_find},
  {"undo",       &MemwatchShell::command_undo},
  {"unfreeze",   &MemwatchShell::command_unfreeze},
  {"unpause",    &MemwatchShell::command_resume},
  {"u",          &MemwatchShell::command_unfreeze},
  {"watch",      &MemwatchShell::command_watch},
  {"wa",         &MemwatchShell::command_watch},
  {"!",          &MemwatchShell::command_watch},
  {"write",      &MemwatchShell::command_write},
  {"wr",         &MemwatchShell::command_write},
  {"w",          &MemwatchShell::command_write},
  {"x",          &MemwatchShell::command_results},
  {"-",          &MemwatchShell::command_disable},
  {"+",          &MemwatchShell::command_enable},
});



// runs a single command
void MemwatchShell::dispatch_command(const string& args_str) {

  // if no command, do nothing
  if (args_str.empty()) {
    return;
  }

  // find the entry in the command table and run the command
  size_t command_end = skip_non_whitespace(args_str, 0);
  size_t args_begin = skip_whitespace(args_str, command_end);
  command_handler_t handler;
  try {
    handler = command_handlers.at(args_str.substr(0, command_end));
  } catch (const out_of_range& e) {
    throw out_of_range("unknown command");
  }

  // run the command
  (this->*handler)(args_str.substr(args_begin));
}

MemwatchShell::MemwatchShell(pid_t pid, uint64_t max_results,
    uint64_t max_search_iterations, bool pause_target, bool interactive,
    bool use_color) : adapter(new ProcessMemoryAdapter(pid)),
    freezer(new RegionFreezer(this->adapter)), pid(pid),
    process_name(name_for_pid(this->pid)), pause_target(pause_target),
    watch(false), interactive(interactive), run(true), use_color(use_color),
    num_commands_run(0), max_results(max_results),
    max_search_iterations(max_search_iterations), name_to_searches(),
    current_search_name("") { }

int MemwatchShell::execute_command(const string& args) {
  try {
    this->dispatch_command(args);
    return 0;
  } catch (const exception& e) {
    printf("error: %s\n", e.what());
    return 1;
  }
}

int MemwatchShell::execute_commands() {

  // initialize history
  using_history();
  string history_filename = get_user_home_directory() + "/.memwatch_history";
  read_history(history_filename.c_str());
  stifle_history(HISTORY_FILE_LENGTH);

  // while we have stuff to do...
  while (this->run) {

    // decide what to prompt the user with (include the search name if a search
    // is currently open)
    // prompt is memwatch:<pid>/<process_name> <num_search>/<num_frozen> <searchname> #
    // or memwatch:<pid>/<process_name> <num_search>/<num_frozen> #
    string prompt;
    try {
      MemorySearch& s = this->get_latest_search();
      const char* search_name = this->current_search_name.empty() ? "(unnamed search)" : this->current_search_name.c_str();
      if (!s.has_memory_snapshot()) {
        prompt = string_printf("memwatch:%u/%s %zus/%zuf %s:%s # ", this->pid,
            this->process_name.c_str(), this->name_to_searches.size(),
            this->freezer->frozen_count(), search_name,
            MemorySearch::short_name_for_search_type(s.get_type()));
      } else if (!s.has_valid_results()) {
        prompt = string_printf("memwatch:%u/%s %zus/%zuf %s:%s(+) # ", this->pid,
            this->process_name.c_str(), this->name_to_searches.size(),
            this->freezer->frozen_count(), search_name,
            MemorySearch::short_name_for_search_type(s.get_type()));
      } else {
        prompt = string_printf("memwatch:%u/%s %zus/%zuf %s:%s(%zu) # ", this->pid,
            this->process_name.c_str(), this->name_to_searches.size(),
            this->freezer->frozen_count(), search_name,
            MemorySearch::short_name_for_search_type(s.get_type()),
            s.get_results().size());
      }

    } catch (const invalid_argument& e) {
      prompt = string_printf("memwatch:%u/%s %zus/%zuf # ", this->pid,
          this->process_name.c_str(), this->name_to_searches.size(),
          this->freezer->frozen_count());
    }

    // read a command and add it to the command history
    // command can be NULL if ctrl+d was pressed - just exit in that case
    char* command = readline(prompt.c_str());
    if (!command) {
      fprintf(stderr, " -- exit\n");
      break;
    }

    // if there's a command, add it to the history
    const char* command_to_execute = command + skip_whitespace(command, 0);
    if (command_to_execute && *command_to_execute) {
      add_history(command);
    }

    // dispatch the command
    try {
      this->execute_command(command_to_execute);
      free(command);
    } catch (...) {
      free(command);
      throw;
    }
  }

  write_history(history_filename.c_str());
  return 0;
}

MemorySearch& MemwatchShell::get_latest_search(const string* name) {
  try {
    auto& searches = this->get_searches(name);
    return searches.at(searches.size() - 1);
  } catch (const out_of_range& e) {
    throw invalid_argument("search does not contain any iterations");
  }
}

vector<MemorySearch>& MemwatchShell::get_searches(const string* name) {
  if (!name) {
    name = &this->current_search_name;
  }
  try {
    return this->name_to_searches.at(*name);
  } catch (const out_of_range& e) {
    throw invalid_argument("search not found, or no search is open");
  }
}

string MemwatchShell::details_for_iteration(const MemorySearch& s) {
  string ret;

  const string& annotation = s.get_annotation();
  if (!annotation.empty()) {
    ret += annotation;
    ret += "; ";
  }

  if (!s.has_memory_snapshot()) {
    ret += "initial";
  } else {
    ret += string_printf("%zu results", s.get_results().size());
  }

  if (s.is_all_memory()) {
    ret += "; all memory";
  }

  return ret;
}
