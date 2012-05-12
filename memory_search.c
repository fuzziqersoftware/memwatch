// memory_search.c, by Martin Michelsen, 2012
// interface for interactive memory search

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "vmmap.h"
#include "vmmap_data.h"
#include "search_data.h"
#include "search_data_list.h"

#include "parse_utils.h"
#include "process_utils.h"
#include "freeze_region.h"

#include "memory_search.h"

// TODO: undoing searches
// TODO: fix file writing to memory
// TODO: show nice memory sizes also in list/dump views

extern int* cancel_var;

struct state {
  pid_t pid;
  char processname[PROCESS_NAME_LENGTH];
  int run;
  MemorySearchDataList* searches;
  MemorySearchData* search; // the current search
};




static int command_help(struct state* st, const char* command) {
  printf(

"memwatch memory search utility\n"
"\n"
"commands:\n"
"  [a]ccess <addr>           enable all access on region containing address\n"
"  [res]ults (or x)          print current search results\n"
"  [l]ist                    list memory regions\n"
"  [d]ump                    dump memory\n"
"  [d]ump <filename>         dump memory regions to <filename>_<addr> files\n"
"  [fi]nd <data>             find occurrences of data\n"
"  [r]ead <addr+size>        read from memory\n"
"  [r]ead !<addr+size>       read from memory every second (ctrl+c to stop)\n"
"  [w]rite <addr> <data>     write to memory\n"
"  wfile <addr> <filename>   write file to memory\n"
"  [f]reeze <addr> <data>    freeze data in memory\n"
"  [f]reeze \"<nm>\" <ad> <dt> freeze data in memory, with given name\n"
"  [u]nfreeze                list frozen regions\n"
"  [u]nfreeze <index>        unfreeze frozen region by index\n"
"  [u]nfreeze <address>      unfreeze frozen region by address\n"
"  [u]nfreeze <name>         unfreeze frozen regions by name\n"
"  [o]pen                    show previous named searches\n"
"  [o]pen <name>             switch to a previous named search\n"
"  [o]pen <type> [name]      begin new search over writable memory only\n"
"  [o]pen !<type> [name]     begin new search over all memory\n"
"  [s]earch <oper> [value]   search for a changed value\n"
"  [c]lose                   delete current search\n"
"  [c]lose <name>            delete search by name\n"
"  regs                      view register contents on all threads\n"
"  wregs <value> <reg>       modify register contents on all threads\n"
"  [b]reak <type> <addr>     set breakpoint on addr\n"
"  pause (or -)              pause process\n"
"  resume (or +)             resume process\n"
"  [sig]nal <signal_number>  send unix signal to process\n"
"  [q]uit                    exit memwatch\n"
"\n"
"<addr+size> may be either <addr(hex)> <size(dec)> or <addr(hex)>:<size(hex)>.\n"
"all <data> arguments are in immediate format (see main usage statement).\n"
"\n"
"the freeze command is like the write command, but the write is repeated in the\n"
"  background until canceled by an unfreeze command.\n"
"new freezes will be named with the same name as the current search (if any),\n"
"  unless a specific name is given\n"
"\n"
"searches done with the search command will increment on the previous search"
"  and narrow down the results. searches done with the find command are one-\n"
"  time searches and don\'t affect the current search results.\n"
"note that a search name is optional: if no name is given, the search is\n"
"  deleted upon switching to a named search or opening a new search.\n"
"\n"
"valid types for open command: u8, u16, u32, u64, s8, s16, s32, s64,\n"
"  float, double, arbitrary.\n"
"any type except arbitrary may be prefixed with l to make it reverse-endian.\n"
"arbitrary may be used to search for a string; i.e. the values being searched\n"
"  for are given in immediate data format.\n"
"reverse-endian searches are usually not necessary. one case in which they may\n"
"  be necessary is when the target is an emulator emulating a reverse-endian\n"
"  system; i.e. emulating a powerpc on an intel machine.\n"
"\n"
"valid operators for search command: = < > <= >= != $\n"
"the $ operator does a flag search (finds values that differ by only one bit).\n"
"  for example, 04 $ 0C is true, but 04 $ 02 is false.\n"
"if the value on the search command is omitted, the previous value (from the\n"
"  last search) is used.\n"
"\n"
"example: find all occurrences of the string \"the quick brown fox\"\n"
"  find \"the quick brown fox\"\n"
"example: read 0x100 bytes from 0x00002F00\n"
"  read 2F00:100\n"
"example: write \"the quick brown fox\" and terminating \\0 to 0x2F00\n"
"  write 2F00 \"the quick brown fox\"00\n"
"example: open a search named \"score\" for a 16-bit signed int\n"
"  open s16 score\n"
"example: search for the value 2160\n"
"  search = 2160\n"
"example: search for values less than 30000\n"
"  search < 30000\n"
"example: search for values greater than or equal to the previous value\n"
"  search >=\n");
  return 0;
}

static int command_list(struct state* st, const char* command) {

  // get the list
  VMRegionDataMap* map = GetProcessRegionList(st->pid, 0);
  if (!map) {
    printf("get process region list failed\n");
    return 0;
  }

  // print it out and delete it
  print_region_map(map);
  DestroyDataMap(map);
  return 0;
}

static int command_dump(struct state* st, const char* command) {

  // dump memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid, 0);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  // save each piece with the given filename
  if (command[0]) {
    char* filename_piece = (char*)malloc(strlen(command) + 64);

    unsigned long x;
    for (x = 0; x < map->numRegions; x++) {

      sprintf(filename_piece, "%s_%016llX", command,
              map->regions[x].region._address);

      // print region info
      printf("%016llX %016llX %c%c%c %s\n",
        map->regions[x].region._address,
        map->regions[x].region._size,
        (map->regions[x].region._attributes & VMREGION_READABLE) ? 'r' : '-',
        (map->regions[x].region._attributes & VMREGION_WRITABLE) ? 'w' : '-',
        (map->regions[x].region._attributes & VMREGION_EXECUTABLE) ? 'x' : '-',
        map->regions[x].data ? filename_piece : "[data not read]");

      // save the file
      if (map->regions[x].data) {
        FILE* f = fopen(filename_piece, "wb");
        fwrite(map->regions[x].data, map->regions[x].region._size, 1, f);
        fclose(f);
      }
    }
    free(filename_piece);
  } else {
    // no filename? then don't save it
    print_region_map(map);
  }

  // delete the data map
  DestroyDataMap(map);
  return 0;
}

static int command_find(struct state* st, const char* command) {

  // dump memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid, 0);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  // read the string
  void* data;
  uint64_t size = read_string_data(command, &data);

  // find the string in a region somewhere
  unsigned long long num_results = 0;
  int x, cont = 1;
  cancel_var = &cont;
  for (x = 0; cont && (x < map->numRegions); x++) {

    // skip regions with no data
    if (!map->regions[x].data)
      continue;

    int y;
    for (y = 0; y <= map->regions[x].region._size - size; y++) {
      if (!memcmp(&map->regions[x].s8_data[y], data, size)) {
        printf("data found at %016llX\n",
              map->regions[x].region._address + y);
        num_results++;
      }
    }
  }
  cancel_var = NULL;
  printf("results: %llu\n", num_results);

  free(data);
  DestroyDataMap(map);
  return 0;
}

static int command_access(struct state* st, const char* command) {

  // read the address
  uint64_t addr;
  sscanf(command, "%llX", &addr);

  // attempt to make it all-access
  if (VMSetRegionProtection(st->pid, addr, 1, VMREGION_ALL, VMREGION_ALL))
    printf("region protection set to all access\n");
  else
    printf("failed to change region protection\n");

  return 0;
}

static int command_read(struct state* st, const char* command) {

  // if a ! is given, make the read repeat
  int cont = (command[0] == '!');
  if (cont)
    command++;

  // read addr/size
  uint64_t addr, size;
  if (!read_addr_size(command, &addr, &size)) {
    printf("invalid command format\n");
    return 0;
  }

  // alloc local read buffer
  void* read_data = malloc(size);
  if (!read_data) {
    printf("failed to allocate memory for reading\n");
    return 0;
  }

  // go ahead and read
  int error;
  cancel_var = &cont;
  do {
    if ((error = VMReadBytes(st->pid, addr, read_data, &size)))
      print_process_data(st->processname, addr, read_data, size);
    else
      printf("failed to read data from process\n");
    if (cont)
      usleep(1000000);
  } while (cont);
  free(read_data);
  cancel_var = NULL;

  return 0;
}

static int command_write(struct state* st, const char* command) {

  // read the address and data
  uint64_t addr, size;
  void* data;
  sscanf(command, "%llX", &addr);
  size = read_string_data(skip_word(command, ' '), &data);

  // and write it
  int error;
  if ((error = VMWriteBytes(st->pid, addr, data, size)))
    printf("wrote %llu (0x%llX) bytes\n", size, size);
  else
    printf("failed to write data to process\n");
  free(data);

  return 0;
}

static int command_write_file(struct state* st, const char* command) {

  // read the parameters
  uint64_t addr;
  sscanf(command, "%llX", &addr);

  // write the file
  write_file_to_process(skip_word(command, ' '), 0, st->pid, addr);

  return 0;
}

static int command_freeze(struct state* st, const char* command) {

  // if a quote is given, read the name
  char* freeze_name = NULL;
  if ((command[0] == '\'') || (command[0] == '\"')) {
    command += copy_quoted_string(command, &freeze_name);
    command = skip_word(command, ' ');
  }

  // read the address
  uint64_t addr, size;
  sscanf(command, "%llX", &addr);
  command = skip_word(command, ' ');

  // read the data
  void* data;
  size = read_string_data(command, &data);

  // and freeze it
  char* use_name = freeze_name ? freeze_name :
                   (st->search ? st->search->name : "[no associated search]");
  if (FreezeRegion(st->pid, addr, size, data, use_name))
    printf("failed to freeze region\n");
  else
    printf("region frozen\n");
  free(data);
  if (freeze_name)
    free(freeze_name);

  return 0;
}

static int command_unfreeze(struct state* st, const char* command) {

  // index given? unfreeze it
  if (command[0]) {
    int num_by_name = UnfreezeRegionByName(command);
    if (num_by_name == 1) {
      printf("region unfrozen\n");
    } else if (num_by_name > 1) {
      printf("%d regions unfrozen\n", num_by_name);
    } else {
      uint64_t addr;
      sscanf(command, "%llX", &addr);
      if (!UnfreezeRegionByAddr(addr))
        printf("region unfrozen\n");
      else {
        sscanf(command, "%llu", &addr);
        if (!UnfreezeRegionByIndex(addr))
          printf("region unfrozen\n");
        else
          printf("failed to unfreeze region\n");
      }
    }

    // else, print frozen regions
  } else
    PrintFrozenRegions(0);

  return 0;
}

static int command_open(struct state* st, const char* command) {

  // read the type. if no type, show the list of searches
  if (!command[0]) {
    PrintSearches(st->searches);
    return 0;
  }

  // check if a search by this name exists or not
  st->search = GetSearchByName(st->searches, command);
  if (st->search) {
    printf("switched to search %s\n", st->search->name);
    return 0;
  }

  // check if a name followed the type
  const char* name = skip_word(command, ' ');

  // if it's S!, we'll search read-only as well (don't know why anyone
  // would want to do this though... :/)
  int search_flags = 0;
  if (command[0] == '!') {
    command++;
    search_flags = SEARCHFLAG_ALLMEMORY;
  }

  // make a new search
  st->search = CreateNewSearchByTypeName(command, name, search_flags);
  if (st->search) {
    AddSearchToList(st->searches, st->search);
    if (st->search->name[0])
      printf("opened new %s search named %s\n",
             GetSearchTypeName(st->search->type), name);
    else
      printf("opened new %s search (unnamed)\n",
             GetSearchTypeName(st->search->type));
  } else
    printf("failed to open new search - did you use a valid typename?\n");

  return 0;
}

static int command_results(struct state* st, const char* command) {
  
  if (!st->search) {
    printf("no search currently open\n");
    return 0;
  }
  if (!st->search->memory) {
    printf("no initial search performed\n");
    return 0;
  }
  int x;
  for (x = 0; x < st->search->numResults; x++)
    printf("%016llX\n", st->search->results[x]);
  printf("results: %lld\n", st->search->numResults);
  
  return 0;
}

static int command_search(struct state* st, const char* command) {

  // make sure a search is opened
  if (!st->search) {
    printf("no search is currently open; use the S command to open a "
           "search\n");
    return 0;
  }

  // read the predicate
  int pred = GetPredicateByName(command);

  // check if a value followed the predicate
  void *value = NULL, *data = NULL;
  uint64_t size = 0;
  const char* value_text = skip_word(command, ' ');
  if (*value_text) {

    // we have a value... blargh
    unsigned long long ivalue;
    float fvalue;
    double dvalue;

    if (IsIntegerSearchType(st->search->type)) {
      sscanf(value_text, "%lld", &ivalue);
      value = &ivalue;
    } else if (st->search->type == SEARCHTYPE_FLOAT ||
               st->search->type == SEARCHTYPE_FLOAT_LE) {
      sscanf(value_text, "%f", &fvalue);
      value = &fvalue;
    } else if (st->search->type == SEARCHTYPE_DOUBLE ||
               st->search->type == SEARCHTYPE_DOUBLE_LE) {
      sscanf(value_text, "%lf", &dvalue);
      value = &dvalue;
    } else if (st->search->type == SEARCHTYPE_DATA) {
      size = read_string_data(value_text, &data);
      value = data;
    }
  }

  // stop the process if necessary, dump memory, and resume the process
  VMRegionDataMap* map = DumpProcessMemory(st->pid,
      st->search->searchflags & SEARCHFLAG_ALLMEMORY ? 0 : VMREGION_WRITABLE);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  // run the search
  MemorySearchData* search_result = ApplyMapToSearch(st->search, map, pred,
                                                     value, size);
  if (data)
    free(data);

  // success? then save the result
  if (!search_result) {
    printf("search failed\n");
    return 0;
  }

  // add this search to the list
  st->search = search_result;
  AddSearchToList(st->searches, st->search);
  if (st->search->numResults >= 20)
    printf("results: %lld\n", st->search->numResults);
  else
    return command_results(st, "");

  return 0;
}

static int command_close(struct state* st, const char* command) {

  // no name present? then we're deleting the current search
  if (!command[0]) {

    // check if there's a current search
    if (!st->search) {
      printf("no search currently open\n");
      return 0;
    }

    // delete the search
    if (st->search->name[0])
      DeleteSearchByName(st->searches, st->search->name);
    else
      DeleteSearch(st->search);
    st->search = NULL;

    // name present? then delete the search by name
  } else {
    // if the search we're deleting is the current search, then the
    // current search should become NULL
    if (st->search && !strcmp(command, st->search->name))
      st->search = NULL;

    // delete the search
    DeleteSearchByName(st->searches, command);
  }

  return 0;
}

static int command_read_regs(struct state* st, const char* command) {

  VMThreadState* thread_state;
  int x, error = VMGetThreadRegisters(st->pid, &thread_state);
  if (error < 0)
    printf("failed to get registers; error %d\n", error);
  else if (error == 0)
    printf("no threads in process\n");
  else {
    for (x = 0; x < error; x++)
      VMPrintThreadRegisters(&thread_state[x]);
    free(thread_state);
  }

  return 0;
}

static int command_write_regs(struct state* st, const char* command) {

  // read the value and regname
  uint64_t regvalue = 0;
  sscanf(command, "%llX", &regvalue);
  command = skip_word(command, ' ');

  // read the thread regs on each thread
  VMThreadState* thread_state;
  int x, error = VMGetThreadRegisters(st->pid, &thread_state);
  if (error < 0)
    printf("failed to get registers; error %d\n", error);
  else if (error == 0)
    printf("no threads in process\n");
  else {

    // change the reg in each thread
    for (x = 0; x < error; x++)
      if (VMSetRegisterValueByName(&thread_state[x], command, regvalue))
        break;
    if (x < error) {
      free(thread_state);
      printf("invalid register name\n");
      return 0;
    }

    // print regs to write
    for (x = 0; x < error; x++)
      VMPrintThreadRegisters(&thread_state[x]);

    // write the reg contents back to the process
    error = VMSetThreadRegisters(st->pid, thread_state, error);
    if (error)
      printf("failed to set registers; error %d\n", error);
    else
      printf("modified thread registers\n");
    free(thread_state);
  }

  return 0;
}

static int command_breakpoint(struct state* st, const char* command) {

  // read the value and regname
  uint64_t addr;
  int type = 0;
  for (; *command && (*command != ' '); command++) {
    if (*command == 'x' || *command == 'X')
      type = 0;
    if (*command == 'w' || *command == 'W')
      type = 1;
    if (*command == 'i' || *command == 'I')
      type = 2;
    if (*command == 'r' || *command == 'R')
      type = 3;
  }
  sscanf(command, "%llX", &addr);

  // read the thread regs on each thread
  VMThreadState* thread_state;
  int x, error = VMGetThreadRegisters(st->pid, &thread_state);
  if (error < 0)
    printf("failed to get registers; error %d\n", error);
  else if (error == 0)
    printf("no threads in process\n");
  else {

    // change the reg in each thread
    for (x = 0; x < error; x++) {
      if (thread_state[x].is64) {
        thread_state[x].db64.__dr0 = addr;
        thread_state[x].db64.__dr7 |= (0x000C0001 | (type << 16));
      } else {
        thread_state[x].db32.__dr0 = addr;
        thread_state[x].db32.__dr7 |= (0x000C0001 | (type << 16));
      }
    }

    // print regs to write
    for (x = 0; x < error; x++)
      VMPrintThreadRegisters(&thread_state[x]);

    // write the reg contents back to the process
    error = VMSetThreadRegisters(st->pid, thread_state, error);
    if (error)
      printf("failed to set registers; error %d\n", error);
    else
      printf("modified thread registers\n");
    free(thread_state);
  }

  return 0;
}

static int command_pause(struct state* st, const char* command) {
  kill(st->pid, SIGSTOP);
  printf("process suspended\n");
  return 0;
}

static int command_resume(struct state* st, const char* command) {
  kill(st->pid, SIGCONT);
  printf("process resumed\n");
  return 0;
}

static int command_signal(struct state* st, const char* command) {
  int sig = atoi(command);
  kill(st->pid, sig);
  if (sig == SIGKILL)
    st->run = 0;
  printf("signal %d sent to process\n", sig);
  return 0;
}

static int command_quit(struct state* st, const char* command) {
  st->run = 0;
  return 0;
}




typedef int (*command_handler)(struct state* st, const char* command);

static const struct {
  const char* word;
  command_handler func;
} command_handlers[] = {
  {"access", command_access},
  {"a", command_access},
  {"break", command_breakpoint},
  {"b", command_breakpoint},
  {"close", command_close},
  {"c", command_close},
  {"dump", command_dump},
  {"d", command_dump},
  {"find", command_find},
  {"fi", command_find},
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
  {"signal", command_signal},
  {"sig", command_signal},
  {"s", command_search},
  {"unfreeze", command_unfreeze},
  {"u", command_unfreeze},
  {"wfile", command_write_file},
  {"wregs", command_write_regs},
  {"write", command_write},
  {"wr", command_write},
  {"w", command_write},
  {"W", command_write_file},
  {"x", command_results},
  {"-", command_pause},
  {"+", command_resume},
  {NULL, NULL}};




// memory searching user interface!
int memory_search(pid_t pid) {

  // construct the initial state
  struct state st;
  memset(&st, 0, sizeof(struct state));
  st.pid = pid;
  st.run = 1;
  st.searches = CreateSearchList();

  // first get the process name
  if (!st.pid)
    strcpy(st.processname, "KERNEL");
  else
    getpidname(st.pid, st.processname, PROCESS_NAME_LENGTH);

  // no process name? process doesn't exist!
  if (!strlen(st.processname)) {
    printf("process %u does not exist\n", st.pid);
    return (-2);
  }

  // init the region freezer
  InitRegionFreezer();

  // while we have stuff to do...
  char* command = NULL;
  while (st.run) {

    // decide what to prompt the user with
    char* prompt;
    if (st.search) {
      prompt = (char*)malloc(30 + strlen(st.processname) +
                             strlen(st.search->name));
      sprintf(prompt, "(memwatch:%u/%s#%s) ", st.pid, st.processname,
              st.search->name);
    } else {
      prompt = (char*)malloc(30 + strlen(st.processname));
      sprintf(prompt, "(memwatch:%u/%s) ", st.pid, st.processname);
    }

    // delete the old command, if present
    if (command)
      free(command);
    command = readline(prompt);
    trim_spaces(command);
    if (command && command[0])
      add_history(command);
    free(prompt);

    // find the entry in the command table
    int command_id, error;
    for (command_id = 0; command_handlers[command_id].word; command_id++)
      if (!strncmp(command_handlers[command_id].word, command,
                   strlen(command_handlers[command_id].word)))
        break;
    if (command_handlers[command_id].func)
      error = command_handlers[command_id].func(&st, skip_word(command, ' '));
    else
      printf("unknown command - try \'help\'\n");
  }

  // shut down the region freezer and return
  DeleteSearchList(st.searches);
  CleanupRegionFreezer();
  return 0;
}
