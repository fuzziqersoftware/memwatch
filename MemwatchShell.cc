#include "MemwatchShell.hh"

#include <assert.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/syslimits.h>
#include <unistd.h>

#include <map>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Process.hh>
#include <phosg/Strings.hh>

#include "Signalable.hh"

using namespace std;

#define HISTORY_FILE_LENGTH  1024


// TODO: undoing searches



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

static vector<string> split_args(const string& args_str, size_t min_args = 0,
    size_t max_args = 0) {

  vector<string> args = split(args_str, ' ');
  if (args.size() < min_args) {
    throw invalid_argument("not enough arguments");
  }
  if (max_args && (args.size() > max_args)) {
    throw invalid_argument("not enough arguments");
  }
  return args;
}

static string read_typed_value(MemorySearch::Type type, const string& s) {
  string data_buffer(sizeof(PrimitiveValue), 0);
  PrimitiveValue* value = (PrimitiveValue*)data_buffer.data();
  if (MemorySearch::is_integer_search_type(type)) {
    value->u64 = stoull(s);
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

void print_regions(FILE* stream, const vector<ProcessMemoryAdapter::Region>& regions) {

  // initialize the counters
  size_t total_error = 0, total_readable = 0, total_writable = 0,
      total_executable = 0, total_accessible = 0, total_inaccessible = 0,
      total_bytes = 0;
  size_t num_error = 0, num_readable = 0, num_writable = 0,
      num_executable = 0, num_accessible = 0, num_inaccessible = 0;

  fprintf(stream, "# addr size access/max_access\n");
  for (const auto& region : regions) {
    fprintf(stream, "%016llX %016llX %c%c%c/%c%c%c%s\n",
        region.addr,
        region.size,
        (region.protection & Protection::READABLE) ? 'r' : '-',
        (region.protection & Protection::WRITABLE) ? 'w' : '-',
        (region.protection & Protection::EXECUTABLE) ? 'x' : '-',
        (region.max_protection & Protection::READABLE) ? 'r' : '-',
        (region.max_protection & Protection::READABLE) ? 'w' : '-',
        (region.max_protection & Protection::READABLE) ? 'x' : '-',
        region.data ? "" : " [data not read]");

    // increment the relevant counters
    if (!region.data) {
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

  fprintf(stream, "# %5lu regions, %s in total\n", regions.size(), total_bytes_str.c_str());
  fprintf(stream, "# %5lu regions, %s unread by memwatch\n", num_error, total_error_str.c_str());
  fprintf(stream, "# %5lu regions, %s accessible\n", num_accessible, total_accessible_str.c_str());
  fprintf(stream, "# %5lu regions, %s readable\n", num_readable, total_readable_str.c_str());
  fprintf(stream, "# %5lu regions, %s writable\n", num_writable, total_writable_str.c_str());
  fprintf(stream, "# %5lu regions, %s executable\n", num_executable, total_executable_str.c_str());
  fprintf(stream, "# %5lu regions, %s inaccessible\n", num_inaccessible, total_inaccessible_str.c_str());
}

uint64_t get_addr_from_command(MemwatchShell& sh, const string& args) {
  if (args[0] == 'x' || args[0] == 's') {
    const auto& s = sh.get_search();

    uint64_t index = stoull(args.substr(1));
    if (index >= s.get_results().size()) {
      throw invalid_argument("search does not have enough results");
    }
    return s.get_results()[index];

  } else if (args[0] == 't') {
    uint64_t index = stoull(args.substr(1));
    if (index >= sh.find_results.size()) {
      throw invalid_argument("search does not have enough results");
    }
    return sh.find_results[index];
  }

  return stoull(args, NULL, 16);
}


////////////////////////////////////////////////////////////////////////////////
// command handlers

// help (no arguments)
static void command_help(MemwatchShell& sh, const string& args) {
  // this is cheating but it means I don't have to maintain two very similar
  // sets of documentation
  int retcode = system("man memwatch");
  if (retcode) {
    throw runtime_error("failed to open the man page - memwatch may not be installed properly on your system");
  }
}

// list (no arguments)
static void command_list(MemwatchShell& sh, const string& args) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    regions = sh.adapter->get_regions();
  }
  print_regions(stdout, regions);
}

// watch <command> [args...]
static void command_watch(MemwatchShell& sh, const string& args) {
  sh.watch = 1;
  sh.execute_command(args);
  sh.watch = 0;
}

// dump [filename_prefix]
static void command_dump(MemwatchShell& sh, const string& args) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    regions = sh.adapter->get_regions(true);
  }

  if (!args.empty()) {
    {
      auto f = fopen_unique(string_printf("%s.index.txt", args.c_str()), "wt");
      print_regions(f.get(), regions);
    }

    for (const auto& region : regions) {
      if (region.data) {
        string filename = string_printf("%s.%016llX.%016llX.bin", args.c_str(),
            region.addr);
        save_file(filename, *region.data);
      }
    }

  } else {
    print_regions(stdout, regions);
  }
}

// find <data>
static void command_find(MemwatchShell& sh, const string& args) {
  vector<ProcessMemoryAdapter::Region> regions;
  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    regions = sh.adapter->get_regions(true);
  }

  string mask;
  string data = parse_data_string(args, &mask);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  sh.find_results.clear();

  for (const auto& region : regions) {
    if (!region.data) {
      continue;
    }

    // search through this region's data for the string
    int y, z;
    for (y = 0; y <= region.size - data.size(); y++) {
      for (z = 0; z < data.size(); z++) {
        if (mask[z] && ((*region.data)[y + z] != data[z])) {
          break;
        }
      }

      if (z == data.size()) {
        printf("(%lu) %016llX (%c%c%c)\n", sh.find_results.size(),
            region.addr + y,
            (region.protection & Protection::READABLE) ? 'r' : '-',
            (region.protection & Protection::WRITABLE) ? 'w' : '-',
            (region.protection & Protection::EXECUTABLE) ? 'x' : '-');
        sh.find_results.emplace_back(region.addr + y);
      }
    }
  }
}

// access <addr> [mode]
static void command_access(MemwatchShell& sh, const string& args_str) {

  auto args = split_args(args_str, 1, 2);

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

  uint64_t addr = get_addr_from_command(sh, args[0]);
  sh.adapter->set_protection(addr, 0, prot, Protection::ALL_ACCESS);
}

// read <addr> <size>
static void command_read(MemwatchShell& sh, const string& args_str) {

  auto args = split_args(args_str, 2, 3);
  uint64_t addr = get_addr_from_command(sh, args[0]);
  uint64_t size = stoull(args[1], NULL, 16);

  // allocate a local read buffer, and a buffer for the previous data if we're
  // repeating the read
  string data(size, 0);
  unique_ptr<string> prev_data(sh.watch ? new string(size, 0) : NULL);

  // go ahead and read the target's memory
  bool first_read = true;
  Signalable s;
  do {
    string time_str = format_time();
    try {
      {
        PauseGuard g(sh.pause_target ? sh.adapter : NULL);
        data = sh.adapter->read(addr, size);
      }

      if (args.size() > 2) {
        save_file(args[2], data);
        printf("%s @ %016llX:%016llX // %s > %s\n", sh.process_name.c_str(),
            addr, size, time_str.c_str(), args[2].c_str());
      } else {
        printf("%s @ %016llX:%016llX // %s\n", sh.process_name.c_str(),
            addr, size, time_str.c_str());
        print_data(stdout, data.data(), data.size(), addr,
            first_read ? NULL : prev_data->data(), sh.use_color);
        printf("\n");
      }

    } catch (const exception& e) {
      printf("%s @ %016llX:%016llX // %s // failed [%s]\n",
          sh.process_name.c_str(), addr, size, time_str.c_str(), e.what());
    }

    if (sh.watch) {
      usleep(1000000); // wait a second before repeating the read
    }

    if (prev_data.get()) {
      prev_data->swap(data);
    }
    first_read = false;

  } while (sh.watch && !s.is_signaled());
}

// write <addr> <data>
static void command_write(MemwatchShell& sh, const string& args) {
  uint64_t addr = get_addr_from_command(sh, args);

  size_t offset = skip_whitespace(args, skip_non_whitespace(args, 0));
  string data = parse_data_string(args.substr(offset));
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  try {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    sh.adapter->write(addr, data);
    printf("wrote %lu (0x%lX) bytes\n", data.size(), data.size());

  } catch (const exception& e) {
    printf("failed to write data to process (%s)\n", e.what());
  }
}

// data <data>
static void command_data(MemwatchShell& sh, const string& args) {
  string mask;
  string data = parse_data_string(args, &mask);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  printf("read %lu (0x%lX) data bytes:\n", data.size(), data.size());
  print_data(stdout, data.data(), data.size());
  printf("read %lu (0x%lX) mask bytes:\n", mask.size(), mask.size());
  print_data(stdout, mask.data(), mask.size());
}

// freeze [n<name>] @<address-or-result-id> <x<data>|s<size>>
// freeze [n<name>] @<address-or-result-id> m<max-entries> <x<data>|s<size>> [Nnull-data]
static void command_freeze(MemwatchShell& sh, const string& args) {

  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  string name;
  uint64_t addr = 0;
  uint64_t array_max_entries = 0;
  uint64_t read_size = 0;
  string data;
  string data_mask;
  string null_data;
  string null_data_mask;
  for (string arg : split_args(args, 2, 0)) {

    if (arg[0] == 'n') {
      name = arg.substr(1);
    } else if (arg[0] == 's') {
      read_size = stoull(arg.substr(1), NULL, 16);
    } else if (arg[0] == 'm') {
      array_max_entries = stoull(arg.substr(1));
    } else if (arg[0] == 'N') {
      null_data = parse_data_string(arg.substr(1), &null_data_mask);
    } else if (arg[0] == 'x') {
      data = parse_data_string(arg.substr(1), &data_mask);
    } else if (arg[0] == '@') {
      addr = get_addr_from_command(sh, arg.substr(1));
    }
  }

  // check the arguments
  if (data.empty() != (bool)read_size) {
    throw invalid_argument("one of data or size must be given");
  }
  if (!null_data.empty() &&
      ((null_data.size() != data.size()) || (null_data.size() != read_size))) {
    throw invalid_argument("data and null data must have the same size");
  }
  if (name.empty()) {
    try {
      sh.get_search();
      name = sh.current_search_name;
    } catch (const out_of_range& e) {
      name = "[no associated search]";
    }
  }

  // read the data if it wasn't given explicitly
  if (read_size) {
    data.clear();
    data.resize(read_size);
    data_mask.clear();
    data_mask.resize(read_size, 0xFF);

    {
      PauseGuard g(sh.pause_target ? sh.adapter : NULL);
      data = sh.adapter->read(addr, read_size);
    }
  }

  // add it to the frozen-list
  if (array_max_entries) {
    if (null_data.empty()) {
      sh.freezer->freeze_array(name, addr, array_max_entries, data, data_mask);
    } else {
      sh.freezer->freeze_array(name, addr, array_max_entries, data, data_mask,
          &null_data, &null_data_mask);
    }
  } else {
    sh.freezer->freeze(name, addr, data);
  }
}

// unfreeze <<name>|<index>|<addr>>
static void command_unfreeze(MemwatchShell& sh, const string& args) {

  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (!args.empty()) {
    // first try to unfreeze by name
    size_t num_regions = sh.freezer->unfreeze_name(args);
    if (num_regions == 1) {
      printf("region unfrozen\n");
      return;
    }
    if (num_regions > 1) {
      printf("%lu regions unfrozen\n", num_regions);
      return;
    }

    // if unfreezing by name didn't match any regions, unfreeze by address
    uint64_t addr = get_addr_from_command(sh, args);
    if (sh.freezer->unfreeze_addr(addr)) {
      printf("region unfrozen\n");
      return;
    }

    // if that didn't work either, try to unfreeze by index
    size_t offset = 0;
    uint64_t index = stoull(args, &offset);
    if (offset == 0) {
      throw invalid_argument("bad argument to unfreeze");
    }
    if (sh.freezer->unfreeze_index(index)) {
      printf("region unfrozen\n");
    } else {
      throw out_of_range("no regions matched");
    }

  // else, print frozen regions
  } else {
    sh.freezer->print_regions(stdout);
  }
}

// frozen [data|commands|cmds]
static void command_frozen(MemwatchShell& sh, const string& args) {

  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args == "data") {
    sh.freezer->print_regions(stdout, true);
  } else if ((args == "cmds") || (args == "commands")) {
    sh.freezer->print_regions_commands(stdout);
  } else {
    sh.freezer->print_regions(stdout);
  }
}

// open
// open <name>
// open <type> [name]
static void command_open(MemwatchShell& sh, const string& args_str) {

  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  vector<string> args = split_args(args_str, 0, 2);

  if (args.size() == 0) {
    for (const auto& it : sh.name_to_search) {
      printf("  %s %s", MemorySearch::name_for_search_type(it.second.get_type()),
          it.first.c_str());
      if (!it.second.has_memory()) {
        printf(" (initial search not done)");
      } else {
        printf(" (%lu results)", it.second.get_results().size());
      }
      if (it.second.is_all_memory()) {
        printf(" (all memory)");
      }
      printf("\n");
    }
    printf("total searches: %lu\n", sh.name_to_search.size());
    return;
  }

  // if the first argument is the name of a search, switch to that search
  if (sh.name_to_search.count(args[0]) && (args.size() == 1)) {
    sh.current_search_name = args[0];
    if (!sh.current_search_name.empty()) {
      sh.name_to_search.erase("");
    }
    printf("switched to search %s\n", sh.current_search_name.c_str());
    return;
  }

  // if the first argument is a valid Type, then open a new search
  MemorySearch::Type type;
  try {
    type = MemorySearch::search_type_for_name(args[0].c_str());
  } catch (const invalid_argument& e) {
    throw invalid_argument("no search named " + args[0]);
  }

  string name = (args.size() == 2) ? args[1] : "";
  bool all_memory = (args[0].find('!') != string::npos);

  sh.name_to_search.erase(name);
  sh.name_to_search.emplace(piecewise_construct, make_tuple(name),
      make_tuple(type, all_memory));
  sh.current_search_name = name;
  if (!sh.current_search_name.empty()) {
    sh.name_to_search.erase("");
  }

  if (sh.current_search_name.empty()) {
    printf("opened new %s search (unnamed)\n", MemorySearch::name_for_search_type(type));
  } else {
    printf("opened new %s search named %s\n", MemorySearch::name_for_search_type(type), name.c_str());
  }
}

// fork <name>
// fork <name1> <name2>
static void command_fork(MemwatchShell& sh, const string& args_str) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = split_args(args_str, 1, 2);
  if (args.size() == 1) {
    // fork the current search into a new one and switch to it
    if (args[0] == sh.current_search_name) {
      throw invalid_argument("can\'t fork a search into itself");
    }
    const MemorySearch& current_search = sh.get_search();

    sh.name_to_search.erase(args[0]);
    sh.name_to_search.emplace(args[0], current_search);
    printf("forked search %s into %s\n", sh.current_search_name.c_str(),
        args[0].c_str());
    sh.current_search_name = args[0];

  } else {
    try {
      sh.name_to_search.erase(args[1]);
      sh.name_to_search.emplace(args[1], sh.name_to_search.at(args[0]));
      printf("forked search %s into %s\n", args[0].c_str(), args[1].c_str());
    } catch (const out_of_range& e) {
      throw out_of_range("no search named " + args[0]);
    }
  }
}

static void print_search_results(MemwatchShell& sh, const MemorySearch& search,
    size_t max_results) {

  // if there's a max given, don't print results if there are more than that
  // many of them
  auto& results = search.get_results();
  if (max_results && (results.size() > max_results)) {
    printf("results: %lu\n", results.size());
    return;
  }

  uint64_t x = 0;
  if (search.get_type() == MemorySearch::Type::DATA) {
    for (uint64_t result : results) {
      printf("(%llu) %016llX\n", x, result);
      x++;
    }

  } else {
    size_t size = MemorySearch::value_size_for_search_type(search.get_type());

    assert(size <= 8);

    for (uint64_t result : results) {
      printf("(%llu) %016llX ", x, result);
      try {
        string data = sh.adapter->read(result, size);
        PrimitiveValue* value = (PrimitiveValue*)data.data();

        switch (search.get_type()) {
          case MemorySearch::Type::UINT8:
            printf("%hhu (0x%02hhX)\n", value->u8, value->u8);
            break;
          case MemorySearch::Type::UINT16_RE:
            value->u16 = bswap16(value->u16);
          case MemorySearch::Type::UINT16:
            printf("%hu (0x%04hX)\n", value->u16, value->u16);
            break;
          case MemorySearch::Type::UINT32_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::UINT32:
            printf("%u (0x%08X)\n", value->u32, value->u32);
            break;
          case MemorySearch::Type::UINT64_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::UINT64:
            printf("%llu (0x%016llX)\n", value->u64, value->u64);
            break;

          case MemorySearch::Type::INT8:
            printf("%hhd (0x%02hhX)\n", value->s8, value->s8);
            break;
          case MemorySearch::Type::INT16_RE:
            value->u16 = bswap16(value->u16);
          case MemorySearch::Type::INT16:
            printf("%hd (0x%04hX)\n", value->s16, value->s16);
            break;
          case MemorySearch::Type::INT32_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::INT32:
            printf("%d (0x%08X)\n", value->s32, value->s32);
            break;
          case MemorySearch::Type::INT64_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::INT64:
            printf("%lld (0x%016llX)\n", value->s64, value->s64);
            break;

          case MemorySearch::Type::FLOAT_RE:
            value->u32 = bswap32(value->u32);
          case MemorySearch::Type::FLOAT:
            printf("%f (0x%08X)\n", value->f, value->u32);
            break;
          case MemorySearch::Type::DOUBLE_RE:
            value->u64 = bswap64(value->u64);
          case MemorySearch::Type::DOUBLE:
            printf("%lf (0x%016llX)\n", value->d, value->u64);
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
static void command_results(MemwatchShell& sh, const string& args) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  MemorySearch& search = sh.get_search(args.empty() ? NULL : &args);

  if (!search.has_memory()) {
    throw invalid_argument("search not found, or no search currently open");
  }

  Signalable s;
  do {
    print_search_results(sh, search, 0);
    if (sh.watch) {
      usleep(1000000); // wait a second, if we're repeating
    }
  } while (sh.watch && !s.is_signaled());
}

// delete <addr1> [addr2]
static void command_delete(MemwatchShell& sh, const string& args_str) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  MemorySearch& search = sh.get_search();

  auto args = split_args(args_str, 1, 2);

  // read the addresses
  uint64_t addr1 = get_addr_from_command(sh, args[0]);
  uint64_t addr2 = (args.size() == 2) ? get_addr_from_command(sh, args[1]) : (addr1 + 1);
  if (addr1 > addr2) {
    throw invalid_argument("range is specified in decreasing order");
  }

  search.delete_results(addr1, addr2);
  print_search_results(sh, search, 20);
}

// search [name] <predicate> [value]
static void command_search(MemwatchShell& sh, const string& args_str) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = split_args(args_str, 1, 3);

  string name = sh.current_search_name;
  MemorySearch::Predicate predicate;
  string value;
  try {
    predicate = MemorySearch::search_predicate_for_name(args[0].c_str());
    if (args.size() > 1) {
      value = args[1];
    }

  } catch (const out_of_range& e) {
    if (args.size() < 2) {
      throw invalid_argument("not enough arguments");
    }
    name = args[0];
    predicate = MemorySearch::search_predicate_for_name(args[1].c_str());
    if (args.size() > 2) {
      value = args[2];
    }
  }

  MemorySearch& search = sh.get_search(&name);

  // convert the value into something we can search for
  if (!value.empty()) {
    value = read_typed_value(search.get_type(), value);
  }

  search.check_can_update(predicate, value);

  shared_ptr<vector<ProcessMemoryAdapter::Region>> regions(new vector<ProcessMemoryAdapter::Region>());
  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    *regions = sh.adapter->get_regions(true);
  }

  search.update(regions, predicate, value, sh.max_results);
  print_search_results(sh, search, 20);
}

// set <value>
static void command_set(MemwatchShell& sh, const string& args) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  MemorySearch& search = sh.get_search();
  if (!search.has_memory()) {
    throw invalid_argument("no initial search performed");
  }

  string data = read_typed_value(search.get_type(), args);
  if (data.empty()) {
    throw invalid_argument("no data was specified");
  }

  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    for (uint64_t result : search.get_results()) {
      try {
        sh.adapter->write(result, data);
        printf("%016llX: wrote %lu (0x%lX) bytes\n", result, data.size(), data.size());
      } catch (const exception& e) {
        printf("%016llX: failed to write data (%s)\n", result, e.what());
      }
    }
  }
}

// close [name]
static void command_close(MemwatchShell& sh, const string& args) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  if (args.empty()) {
    if (!sh.name_to_search.erase(sh.current_search_name)) {
      throw invalid_argument("no search currently open");
    }
    sh.current_search_name.clear();

  } else {
    if (!sh.name_to_search.erase(args)) {
      throw invalid_argument("no open search named " + args);
    }
    if (sh.current_search_name == args) {
      sh.current_search_name.clear();
    }
  }
}

#define print_reg_value(str, state, prev, regname) \
  do { \
    int diff = ((prev) && ((state).regname != (prev)->regname)); \
    if (sh.use_color && diff) { \
      print_color_escape(stdout, TerminalFormat::BOLD, TerminalFormat::FG_RED, \
          TerminalFormat::END); \
    } \
    printf((str), (state).regname); \
    if (sh.use_color && diff) { \
      print_color_escape(stdout, TerminalFormat::NORMAL, TerminalFormat::END); \
    } \
  } while (0);

// prints the register contents in a thread state
static void print_thread_regs(MemwatchShell& sh, int tid,
    const ProcessMemoryAdapter::ThreadState& state,
    const ProcessMemoryAdapter::ThreadState* prev) {

  if (prev && (state.is64 != prev->is64)) {
    printf("warning: previous thread state does not match architecture of current state\n");
    prev = NULL;
  }

  if (!state.is64) {
    printf("[%d: thread 32-bit @ ", tid);
    print_reg_value("eip:%08X", state, prev, st32.__eip);
    printf("]\n");
    print_reg_value("  eax: %08X", state, prev, st32.__eax);
    print_reg_value("  ecx: %08X", state, prev, st32.__ecx);
    print_reg_value("  edx: %08X", state, prev, st32.__edx);
    print_reg_value("  ebx: %08X\n", state, prev, st32.__ebx);
    print_reg_value("  ebp: %08X", state, prev, st32.__ebp);
    print_reg_value("  esp: %08X", state, prev, st32.__esp);
    print_reg_value("  esi: %08X", state, prev, st32.__esi);
    print_reg_value("  edi: %08X\n", state, prev, st32.__edi);
    print_reg_value("  eflags: %08X\n", state, prev, st32.__eflags);
    print_reg_value("  cs:  %08X", state, prev, st32.__cs);
    print_reg_value("  ds:  %08X", state, prev, st32.__ds);
    print_reg_value("  es:  %08X", state, prev, st32.__es);
    print_reg_value("  fs:  %08X\n", state, prev, st32.__fs);
    print_reg_value("  gs:  %08X", state, prev, st32.__gs);
    print_reg_value("  ss:  %08X\n", state, prev, st32.__ss);
    // TODO: print floating state
    print_reg_value("  dr0: %08X", state, prev, db32.__dr0);
    print_reg_value("  dr1: %08X", state, prev, db32.__dr1);
    print_reg_value("  dr2: %08X", state, prev, db32.__dr2);
    print_reg_value("  dr3: %08X\n", state, prev, db32.__dr3);
    print_reg_value("  dr4: %08X", state, prev, db32.__dr4);
    print_reg_value("  dr5: %08X", state, prev, db32.__dr5);
    print_reg_value("  dr6: %08X", state, prev, db32.__dr6);
    print_reg_value("  dr7: %08X\n", state, prev, db32.__dr7);

  } else {
    printf("[%d: thread 64-bit @ ", tid);
    print_reg_value("rip:%016llX", state, prev, st64.__rip);
    printf("]\n");
    print_reg_value("  rax: %016llX",      state, prev, st64.__rax);
    print_reg_value("  rcx: %016llX",      state, prev, st64.__rcx);
    print_reg_value("  rdx: %016llX\n",    state, prev, st64.__rdx);
    print_reg_value("  rbx: %016llX",      state, prev, st64.__rbx);
    print_reg_value("  rbp: %016llX",      state, prev, st64.__rbp);
    print_reg_value("  rsp: %016llX\n",    state, prev, st64.__rsp);
    print_reg_value("  rsi: %016llX",      state, prev, st64.__rsi);
    print_reg_value("  rdi: %016llX",      state, prev, st64.__rdi);
    print_reg_value("  r8:  %016llX\n",    state, prev, st64.__r8);
    print_reg_value("  r9:  %016llX",      state, prev, st64.__r9);
    print_reg_value("  r10: %016llX",      state, prev, st64.__r10);
    print_reg_value("  r11: %016llX\n",    state, prev, st64.__r11);
    print_reg_value("  r12: %016llX",      state, prev, st64.__r12);
    print_reg_value("  r13: %016llX",      state, prev, st64.__r13);
    print_reg_value("  r14: %016llX\n",    state, prev, st64.__r14);
    print_reg_value("  r15: %016llX  ",    state, prev, st64.__r15);
    print_reg_value("  rflags: %016llX\n", state, prev, st64.__rflags);
    // TODO: print floating state
    print_reg_value("  cs:  %016llX",      state, prev, st64.__cs);
    print_reg_value("  fs:  %016llX",      state, prev, st64.__fs);
    print_reg_value("  gs:  %016llX\n",    state, prev, st64.__gs);
    print_reg_value("  dr0: %016llX",      state, prev, db64.__dr0);
    print_reg_value("  dr1: %016llX",      state, prev, db64.__dr1);
    print_reg_value("  dr2: %016llX\n",    state, prev, db64.__dr2);
    print_reg_value("  dr3: %016llX",      state, prev, db64.__dr3);
    print_reg_value("  dr4: %016llX",      state, prev, db64.__dr4);
    print_reg_value("  dr5: %016llX\n",    state, prev, db64.__dr5);
    print_reg_value("  dr6: %016llX",      state, prev, db64.__dr6);
    print_reg_value("  dr7: %016llX\n",    state, prev, db64.__dr7);
  }
}

// regs
static void command_read_regs(MemwatchShell& sh, const string& args_str) {

  Signalable s;
  do {
    unordered_map<mach_port_t, ProcessMemoryAdapter::ThreadState> state;
    unordered_map<mach_port_t, ProcessMemoryAdapter::ThreadState> prev;
    {
      PauseGuard g(sh.pause_target ? sh.adapter : NULL);
      state = sh.adapter->get_threads_registers();
    }

    string time_str = format_time();
    printf("%s [%lu threads] // %s\n", sh.process_name.c_str(), state.size(),
        time_str.c_str());

    for (const auto& it : state) {
      try {
        print_thread_regs(sh, it.first, it.second, &prev.at(it.first));
      } catch (const out_of_range& e) {
        print_thread_regs(sh, it.first, it.second, NULL);
      }
    }

    state.swap(prev);
    state.clear();

    if (sh.watch) {
      usleep(1000000); // wait a second, if the read is repeating
    }
  } while (sh.watch && !s.is_signaled());
}

// wregs <tid> <reg> <value>
static void command_write_regs(MemwatchShell& sh, const string& args_str) {
  if (!sh.interactive) {
    throw invalid_argument("this command can only be used in interactive mode");
  }

  auto args = split_args(args_str, 3, 3);

  int tid = stoi(args[0]);
  auto& regname = args[1];
  uint64_t value = stoull(args[2]);

  {
    PauseGuard g(sh.pause_target ? sh.adapter : NULL);
    auto data = sh.adapter->get_threads_registers();
    try {
      data.at(tid).set_register_by_name(regname.c_str(), value);
    } catch (const out_of_range& e) {
      throw out_of_range("no such thread/register");
    }
    sh.adapter->set_threads_registers(data);
  }
}

// stacks [size]
static void command_read_stacks(MemwatchShell& sh, const string& args_str) {

  uint64_t size = args_str.empty() ? 0x100 : stoull(args_str, NULL, 16);

  Signalable s;
  do {
    string read_data(size, 0);
    {
      PauseGuard g(sh.pause_target ? sh.adapter : NULL);
      string time_str = format_time();
      auto regs = sh.adapter->get_threads_registers();
      for (const auto& it : regs) {
        uint64_t addr = it.second.is64 ? it.second.st64.__rsp : it.second.st32.__esp;
        printf("%s [Thread %d] @ %016llX:%016llX // %s\n",
            sh.process_name.c_str(), it.first, addr, size, time_str.c_str());
        try {
          string data = sh.adapter->read(addr, size);
          print_data(stdout, read_data.data(), read_data.size(), addr);
        } catch (const exception& e) {
          printf("failed to read data (%s)\n", e.what());
        }
      }
    }

    if (sh.watch) {
      usleep(1000000); // wait a second, if the read is repeating
    }
  } while (sh.watch && !s.is_signaled());
}

// pause
static void command_pause(MemwatchShell& sh, const string& args_str) {
  sh.adapter->pause();
}

// resume
static void command_resume(MemwatchShell& sh, const string& args_str) {
  sh.adapter->resume();
}

// signal <signum>
static void command_signal(MemwatchShell& sh, const string& args_str) {
  int signum = stoi(args_str.c_str());
  if (kill(sh.pid, signum)) {
    throw runtime_error(string_printf("failed to send signal %d to process", signum));
  }
}

// attach [name_or_pid]
static void command_attach(MemwatchShell& sh, const string& args_str) {

  // if no command given, use the current process name
  // (i.e. if the program was quit & restarted, its pid will change)
  const string& name = args_str.empty() ? sh.process_name : args_str;

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
  sh.pid = new_pid;
  sh.process_name = new_name;
  sh.adapter->attach(sh.pid);
}

// state [<field> <value>]
static void command_state(MemwatchShell& sh, const string& args_str) {

  if (args_str.empty()) {
    printf("use_color = %d\n", sh.use_color);
    printf("pid = %d\n", sh.pid);
    printf("process_name = \"%s\"\n", sh.process_name.c_str());
    printf("pause_target = %d\n", sh.pause_target);
    printf("max_results = %llu\n", sh.max_results);
    printf("interactive = %d\n", sh.interactive);
    printf("run = %d\n", sh.run);
    printf("num_commands_run = %llu\n", sh.num_commands_run);
    printf("num_find_results = %lu\n", sh.find_results.size());
    return;
  }

  auto args = split_args(args_str, 2, 2);

  if (args[0] == "use_color") {
    sh.use_color = stoi(args[1]);
  } else if (args[0] == "process_name") {
    sh.process_name = args[1];
  } else if (args[0] == "pause_target") {
    sh.pause_target = stoi(args[1]);
  } else if (args[0] == "max_results") {
    sh.max_results = stoull(args[1]);
  } else if (args[0] == "run") {
    sh.run = stoi(args[1]);
  } else {
    throw invalid_argument("unknown or read-only field");
  }
}

// quit
static void command_quit(MemwatchShell& sh, const string& args_str) {
  sh.run = false;
}



// list of pointers to functions for each command
typedef void (*command_handler_t)(MemwatchShell& sh, const string& args_str);

static const unordered_map<string, command_handler_t> command_handlers({
  {"access", command_access},
  {"acc", command_access},
  {"a", command_access},
  {"attach", command_attach},
  {"att", command_attach},
  {"at", command_attach},
  {"close", command_close},
  {"c", command_close},
  {"data", command_data},
  {"delete", command_delete},
  {"del", command_delete},
  {"dump", command_dump},
  {"d", command_dump},
  {"find", command_find},
  {"fi", command_find},
  {"fork", command_fork},
  {"fk", command_fork},
  {"fo", command_fork},
  {"frozen", command_frozen},
  {"fzn", command_frozen},
  {"freeze", command_freeze},
  {"fr", command_freeze},
  {"f", command_freeze},
  {"help", command_help},
  {"h", command_help},
  {"list", command_list},
  {"l", command_list},
  {"open", command_open},
  {"o", command_open},
  {"pause", command_pause},
  {"quit", command_quit},
  {"q", command_quit},
  {"read", command_read},
  {"rd", command_read},
  {"regs", command_read_regs},
  {"results", command_results},
  {"resume", command_resume},
  {"res", command_results},
  {"r", command_read},
  {"search", command_search},
  {"s", command_search},
  {"set", command_set},
  {"signal", command_signal},
  {"sig", command_signal},
  {"stacks", command_read_stacks},
  {"stax", command_read_stacks},
  {"stx", command_read_stacks},
  {"state", command_state},
  {"st", command_state},
  {"t", command_find},
  {"unfreeze", command_unfreeze},
  {"u", command_unfreeze},
  {"watch", command_watch},
  {"wa", command_watch},
  {"!", command_watch},
  {"wregs", command_write_regs},
  {"write", command_write},
  {"wr", command_write},
  {"w", command_write},
  {"x", command_results},
  {"-", command_pause},
  {"+", command_resume},
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
    throw out_of_range("unknown command - try \'help\'");
  }

  // run the command
  handler(*this, args_str.substr(args_begin));
}

MemwatchShell::MemwatchShell(pid_t pid, uint64_t max_results, bool pause_target,
    bool interactive, bool use_color) : adapter(new ProcessMemoryAdapter(pid)),
    freezer(new RegionFreezer(this->adapter)), pid(pid),
    process_name(name_for_pid(this->pid)), pause_target(pause_target),
    watch(false), interactive(interactive), run(true), use_color(use_color),
    num_commands_run(0), max_results(max_results), name_to_search(),
    current_search_name("") { }

int MemwatchShell::execute_command(const string& args) {
  try {
    this->dispatch_command(args);
    return 0;
  } catch (const exception& e) {
    fprintf(stderr, "error: %s\n", e.what());
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
  unique_ptr<char> command;
  while (this->run) {

    // decide what to prompt the user with (include the seearch name if a search
    // is currently open)
    // prompt is memwatch:<pid>/<process_name> <num_search>/<num_frozen> <searchname> #
    // or memwatch:<pid>/<process_name> <num_search>/<num_frozen> #
    string prompt;
    try {
      MemorySearch& s = this->get_search();
      const char* search_name = this->current_search_name.empty() ? "(unnamed search)" : this->current_search_name.c_str();
      if (!s.has_memory()) {
        prompt = string_printf("memwatch:%u/%s %ds/%df %s:%s # ", this->pid,
            this->process_name.c_str(), this->name_to_search.size(),
            this->freezer->frozen_count(), search_name,
            MemorySearch::name_for_search_type(s.get_type()));
      } else {
        prompt = string_printf("memwatch:%u/%s %ds/%df %s:%s(%llu) # ", this->pid,
            this->process_name.c_str(), this->name_to_search.size(),
            this->freezer->frozen_count(), search_name,
            MemorySearch::name_for_search_type(s.get_type()),
            s.get_results().size());
      }

    } catch (const invalid_argument& e) {
      prompt = string_printf("memwatch:%u/%s %ds/%df # ", this->pid,
          this->process_name.c_str(), this->name_to_search.size(),
          this->freezer->frozen_count());
    }

    // read a command and add it to the command history
    // command can be NULL if ctrl+d was pressed - just exit in that case
    command.reset(readline(prompt.c_str()));
    if (!command.get()) {
      printf(" -- exit\n");
      break;
    }

    // if there's a command, add it to the history
    const char* command_to_execute = command.get() + skip_whitespace(command.get(), 0);
    if (command_to_execute && *command_to_execute) {
      add_history(command.get());
    }

    // dispatch the command
    this->execute_command(command_to_execute);
  }

  write_history(history_filename.c_str());
  return 0;
}

MemorySearch& MemwatchShell::get_search(const string* name) {
  if (!name) {
    name = &this->current_search_name;
  }
  try {
    return this->name_to_search.at(*name);
  } catch (const out_of_range& e) {
    throw invalid_argument("search not found, or no search is open");
  }
}
