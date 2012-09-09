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

// this variable is set to 1 by the main thread when the user presses CTRL+C;
// if this happens, the current operation should cancel itself
extern int* cancel_var;

// struct to contain all of the mutable state of the memory search utility
struct state {
  pid_t pid; // process id of the process that's being operated on
  char processname[PROCESS_NAME_LENGTH]; // name of that process
  int freeze_while_operating;
  int run; // set to 0 to exit the memory search interface
  MemorySearchDataList* searches; // list of open searches
  MemorySearchData* search; // the current search
};



// print out a help message
static int command_help(struct state* st, const char* command) {
  printf(
"memwatch memory search utility\n"
"\n"
"memory commands:\n"
"  [a]ccess <addr>           enable all access on region containing address\n"
"  [l]ist                    list memory regions\n"
"  [d]ump                    dump memory\n"
"  [d]ump <filename>         dump memory regions to <filename>_<addr> files\n"
"  [r]ead <addr+size>        read from memory\n"
"  [r]ead !<addr+size>       read from memory every second (ctrl+c to stop)\n"
"  [w]rite <addr> <data>     write to memory\n"
"  wfile <addr> <filename>   write file to memory\n"
"search commands:\n"
"  [fi]nd <data>             find occurrences of data\n"
"  [o]pen                    show previous named searches\n"
"  [o]pen <name>             switch to a previous named search\n"
"  [o]pen <type> [name]      begin new search over writable memory only\n"
"  [o]pen !<type> [name]     begin new search over all memory\n"
"  [c]lose                   delete current search\n"
"  [c]lose <name>            delete search by name\n"
"  [s]earch <oper> [value]   search for a changed value\n"
"  [res]ults (or x)          print current search results\n"
"  set <value>               write <value> to all current result locations\n"
"  [del]ete <addr1> [addr2]  delete result at addr1, or between addr1 and addr2\n"
"freeze commands:\n"
"  [f]reeze <addr> <data>    freeze data in memory\n"
"  [f]reeze \"<name>\" <addr> <data> freeze data in memory, with given name\n"
"  [u]nfreeze                list frozen regions\n"
"  [u]nfreeze <index>        unfreeze frozen region by index\n"
"  [u]nfreeze <address>      unfreeze frozen region by address\n"
"  [u]nfreeze <name>         unfreeze frozen regions by name\n"
"execution state commands:\n"
"  pause (or -)              pause process\n"
"  resume (or +)             resume process\n"
"  [k]ill                    terminate process\n"
"  [sig]nal <signal_number>  send unix signal to process\n"
"  regs                      view register contents on all threads\n"
"  wregs <value> <reg>       modify register contents on all threads\n"
"  [b]reak <sizetype> <addr> set breakpoint on addr\n"
"general commands:\n"
"  [at]tach <pid_or_name>    attach to a new process, keeping freezes & searches\n"
"  [h]elp                    display help message\n"
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
"searches done with the search command will increment on the previous search\n"
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

// list regions of memory in the target process
static int command_list(struct state* st, const char* command) {

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // get the list
  VMRegionDataMap* map = GetProcessRegionList(st->pid, 0);
  if (!map) {
    printf("get process region list failed\n");
    return 0;
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // print it out and delete it, then return
  print_region_map(map);
  DestroyDataMap(map);
  return 0;
}

// take a snapshot of the target process' memory and save it to disk
static int command_dump(struct state* st, const char* command) {

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // dump memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid, 0);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // save each region's data with the given filename prefix
  if (command[0]) {

    // allocate space for the chunk filename
    char* filename_piece = (char*)malloc(strlen(command) + 64);

    // for each region...
    unsigned long x;
    for (x = 0; x < map->numRegions; x++) {

      // build the filename: prefix_address
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

    // free the allocated filename space
    free(filename_piece);

  // no filename prefix given? then don't save the data
  } else
    print_region_map(map);

  // delete the data map
  DestroyDataMap(map);
  return 0;
}

// find the given data in the target's memory
static int command_find(struct state* st, const char* command) {

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // take a snapshot of the target's memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid, 0);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // read the «search string from the command
  void* data;
  uint64_t size = read_string_data(command, &data);

  // loop through the regions, searching for the string
  unsigned long long num_results = 0;
  int x, cont = 1;
  cancel_var = &cont; // this operation can be canceled
  for (x = 0; cont && (x < map->numRegions); x++) {

    // skip regions with no data
    if (!map->regions[x].data)
      continue;

    // search through this region's data for the string
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

  // print the result count, clean up and return
  printf("results: %llu\n", num_results);
  free(data);
  DestroyDataMap(map);
  return 0;
}

// set a region's permissions to all access
static int command_access(struct state* st, const char* command) {

  // read the address
  uint64_t addr;
  sscanf(command, "%llX", &addr);

  // attempt to change the access permissions
  if (VMSetRegionProtection(st->pid, addr, 1, VMREGION_ALL, VMREGION_ALL))
    printf("region protection set to all access\n");
  else
    printf("failed to change region protection\n");

  return 0;
}

// read data from targer memory
static int command_read(struct state* st, const char* command) {

  // if a ! is given in the command, make the read repeat
  int cont = (command[0] == '!');
  if (cont)
    command++;

  // read addr/size from the command
  uint64_t addr, size;
  if (!read_addr_size(command, &addr, &size)) {
    printf("invalid command format\n");
    return 0;
  }

  // allocate a local read buffer, and a buffer for the previous data if we're
  // repeating the read
  void* read_data = malloc(size);
  void* read_data_prev = cont ? malloc(size) : NULL;
  // if either allocation failed, report the failure, clean up & return
  if (!read_data || (cont && !read_data_prev)) {
    printf("failed to allocate memory for reading\n");
    if (read_data)
      free(read_data);
    if (read_data_prev)
      free(read_data_prev);
    return 0;
  }

  // go ahead and read the target's memory
  int error, times = 0;
  cancel_var = &cont;
  do {
    if (st->freeze_while_operating)
      VMPauseProcess(st->pid);

    if ((error = VMReadBytes(st->pid, addr, read_data, &size)))
      print_process_data(st->processname, addr, read_data,
                         times ? read_data_prev : NULL, size);
    else
      printf("failed to read data from process\n");

    if (st->freeze_while_operating)
      VMResumeProcess(st->pid);

    if (cont)
      usleep(1000000); // wait a second, if the read is repeating

    // swap the current & previous read buffers
    void* tmp = read_data_prev;
    read_data_prev = read_data;
    read_data = tmp;
    times++;
  } while (cont);

  // clean up & return
  if (read_data)
    free(read_data);
  if (read_data_prev)
    free(read_data_prev);
  cancel_var = NULL;
  return 0;
}

// write data to target's memory
static int command_write(struct state* st, const char* command) {

  // read the address and data from the command string
  uint64_t addr, size;
  void* data;
  sscanf(command, "%llX", &addr);
  size = read_string_data(skip_word(command, ' '), &data);

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // write the data to the target's memory
  int error;
  if ((error = VMWriteBytes(st->pid, addr, data, size)))
    printf("wrote %llu (0x%llX) bytes\n", size, size);
  else
    printf("failed to write data to process\n");

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // clean up & return
  free(data);
  return 0;
}

// write a file to the target's memory
static int command_write_file(struct state* st, const char* command) {

  // read the address
  uint64_t addr;
  sscanf(command, "%llX", &addr);

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // write the file
  write_file_to_process(skip_word(command, ' '), 0, st->pid, addr);

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  return 0;
}

// add a region to the frozen-list with the given data
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

  // add it to the frozen-list
  char* use_name = freeze_name ? freeze_name :
                   (st->search ? st->search->name : "[no associated search]");
  if (FreezeRegion(st->pid, addr, size, data, use_name))
    printf("failed to freeze region\n");
  else
    printf("region frozen\n");

  // clean up & return
  free(data);
  if (freeze_name)
    free(freeze_name);
  return 0;
}

// remove a region from the frozen-list
static int command_unfreeze(struct state* st, const char* command) {

  // index given? unfreeze a region
  if (command[0]) {
    // first try to unfreeze by name
    int num_by_name = UnfreezeRegionByName(command);
    if (num_by_name == 1) {
      printf("region unfrozen\n");
    } else if (num_by_name > 1) {
      printf("%d regions unfrozen\n", num_by_name);
    } else {

      // if unfreezing by name didn't match any regions, unfreeze by address
      uint64_t addr;
      sscanf(command, "%llX", &addr);
      if (!UnfreezeRegionByAddr(addr))
        printf("region unfrozen\n");
      else {

        // if that didn't work either, try to unfreeze by index
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

// open a new search
static int command_open(struct state* st, const char* command) {

  // if no argument given, just show the list of searches
  if (!command[0]) {
    PrintSearches(st->searches);
    return 0;
  }

  // check if a search by the given name exists or not
  MemorySearchData* s = GetSearchByName(st->searches, command);
  if (s) {
    st->search = s;
    printf("switched to search %s\n", st->search->name);
    return 0;
  }

  // if a ! is given, we'll search read-only as well. don't know why anyone
  // would want to do this though... :/
  int search_flags = 0;
  if (command[0] == '!') {
    command++;
    search_flags = SEARCHFLAG_ALLMEMORY;
  }

  // check if a name followed the type
  const char* name = skip_word(command, ' ');

  // make a new search
  s = CreateNewSearchByTypeName(command, name, search_flags);
  if (s) {
    st->search = s;

    // add this search to the list of open searches
    AddSearchToList(st->searches, st->search);
    if (st->search->name[0])
      printf("opened new %s search named %s\n",
             GetSearchTypeName(st->search->type), name);
    else
      printf("opened new %s search (unnamed)\n",
             GetSearchTypeName(st->search->type));

  // report failure if none of the above happened
  } else
    printf("failed to open new search - did you use a valid typename?\n");

  return 0;
}

// print list of results for current search
static int command_results(struct state* st, const char* command) {

  // can't do this if there isn't a current search, or if the current search
  // object has no memory associated with it (and therefore no valid results)
  if (!st->search) {
    printf("no search currently open\n");
    return 0;
  }
  if (!st->search->memory) {
    printf("no initial search performed\n");
    return 0;
  }

  // print the results!
  int x;
  if (st->search->type == SEARCHTYPE_DATA) {
    for (x = 0; x < st->search->numResults; x++)
      printf("%016llX\n", st->search->results[x]);
  } else {
    mach_vm_size_t size = SearchDataSize(st->search->type);
    void* data = malloc(size);
    for (x = 0; x < st->search->numResults; x++) {
      printf("%016llX ", st->search->results[x]);
      if (!VMReadBytes(st->pid, st->search->results[x], data, &size))
        printf("<< memory not readable >>\n");
      else {
        if (IsReverseEndianSearchType(st->search->type))
          bswap(data, size);
        switch (st->search->type) {
          case SEARCHTYPE_UINT8:
            printf("%hhu (0x%02hhX)\n", *(uint8_t*)data, *(uint8_t*)data);
            break;
          case SEARCHTYPE_UINT16:
          case SEARCHTYPE_UINT16_LE:
            printf("%hu (0x%04hX)\n", *(uint16_t*)data, *(uint16_t*)data);
            break;
          case SEARCHTYPE_UINT32:
          case SEARCHTYPE_UINT32_LE:
            printf("%u (0x%08X)\n", *(uint32_t*)data, *(uint32_t*)data);
            break;
          case SEARCHTYPE_UINT64:
          case SEARCHTYPE_UINT64_LE:
            printf("%llu (0x%016llX)\n", *(uint64_t*)data, *(uint64_t*)data);
            break;
          case SEARCHTYPE_INT8:
            printf("%hhd (0x%02hhX)\n", *(int8_t*)data, *(uint8_t*)data);
            break;
          case SEARCHTYPE_INT16:
          case SEARCHTYPE_INT16_LE:
            printf("%hd (0x%04hX)\n", *(int16_t*)data, *(uint16_t*)data);
            break;
          case SEARCHTYPE_INT32:
          case SEARCHTYPE_INT32_LE:
            printf("%d (0x%08X)\n", *(int32_t*)data, *(uint32_t*)data);
            break;
          case SEARCHTYPE_INT64:
          case SEARCHTYPE_INT64_LE:
            printf("%lld (0x%016llX)\n", *(int64_t*)data, *(uint64_t*)data);
            break;
          case SEARCHTYPE_FLOAT:
          case SEARCHTYPE_FLOAT_LE:
            printf("%f (0x%08X)\n", *(float*)data, *(uint32_t*)data);
            break;
          case SEARCHTYPE_DOUBLE:
          case SEARCHTYPE_DOUBLE_LE:
            printf("%lf (0x%016llX)\n", *(double*)data, *(uint64_t*)data);
            break;
        }
      }
    }
    if (data)
      free(data);
  }
  printf("results: %lld\n", st->search->numResults);

  return 0;
}

// delete specific results from a search
static int command_delete(struct state* st, const char* command) {

  // can't do this if there isn't a current search, or if the current search
  // object has no memory associated with it (and therefore no valid results)
  if (!st->search) {
    printf("no search currently open\n");
    return 0;
  }
  if (!st->search->memory) {
    printf("no initial search performed\n");
    return 0;
  }

  // read the addresses
  uint64_t addr1 = 0, addr2 = 0;
  sscanf(command, "%llX %llX", &addr1, &addr2);

  // if only 1 address given, addr2 will be 0, so just make a 1-byte range
  if (addr2 < addr1)
    addr2 = addr1 + 1;

  // delete search results in the given range
  DeleteResults(st->search, addr1, addr2);

  // if there aren't many results left, print them out... otherwise, just print
  // the number of remaining results
  if (st->search->numResults < 20)
    return command_results(st, "");
  printf("results: %lld\n", st->search->numResults);

  return 0;
}

// execute a search using the current search object
static int command_search(struct state* st, const char* command) {

  // make sure a search is opened
  if (!st->search) {
    printf("no search is currently open; use the o command to open a "
           "search\n");
    return 0;
  }

  // read the predicate
  int pred = GetPredicateByName(command);

  // check if a value followed the predicate
  void *value = NULL, *data = NULL;
  unsigned long long ivalue;
  float fvalue;
  double dvalue;
  uint64_t size = 0;
  const char* value_text = skip_word(command, ' ');
  if (*value_text) {

    // we have a value... read it in
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

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // take a snapshot of the target's memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid,
      st->search->searchflags & SEARCHFLAG_ALLMEMORY ? 0 : VMREGION_WRITABLE);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // run the search, then delete the data (if allocated)
  MemorySearchData* search_result = ApplyMapToSearch(st->search, map, pred,
                                                     value, size);
  if (data)
    free(data);

  // success? then save the result
  if (!search_result) {
    printf("search failed\n");
    return 0;
  }

  // add this search to the list. don't need to delete the old one here - it'll
  // be deleted by AddSearchToList if it gets replaced by the new one
  st->search = search_result;
  AddSearchToList(st->searches, st->search);

  // print results if there aren't too many of them
  if (st->search->numResults < 20)
    return command_results(st, "");
  printf("results: %lld\n", st->search->numResults);
  return 0;
}

// write a value to all current search results
static int command_set(struct state* st, const char* command) {

  // need an open search with valid results to do this
  if (!st->search) {
    printf("no search currently open\n");
    return 0;
  }
  if (!st->search->memory) {
    printf("no initial search performed\n");
    return 0;
  }

  // read the value, matching the search type
  int64_t ivalue;
  double dvalue;
  float fvalue;
  void* datavalue = NULL;
  void* data;
  unsigned int size;
  if (IsIntegerSearchType(st->search->type)) {
    sscanf(command, "%lld", &ivalue);
    data = &ivalue;
    size = SearchDataSize(st->search->type);
  } else if ((st->search->type == SEARCHTYPE_FLOAT) ||
             (st->search->type == SEARCHTYPE_FLOAT_LE)) {
    sscanf(command, "%g", &fvalue);
    data = &fvalue;
    size = SearchDataSize(st->search->type);
  } else if ((st->search->type == SEARCHTYPE_DOUBLE) ||
             (st->search->type == SEARCHTYPE_DOUBLE_LE)) {
    sscanf(command, "%lg", &dvalue);
    data = &dvalue;
    size = SearchDataSize(st->search->type);
  } else if (st->search->type == SEARCHTYPE_DATA) {
    size = read_string_data(command, &datavalue);
    data = datavalue;
  } else {
    printf("error: unknown search type\n");
    return 0;
  }

  // byteswap the value if the search is a reverse-endian type
  if (IsReverseEndianSearchType(st->search->type))
    bswap(data, size);

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // write the data to each result address
  int x, error;
  for (x = 0; x < st->search->numResults; x++) {
    uint64_t addr = st->search->results[x];
    if ((error = VMWriteBytes(st->pid, addr, data, size)))
      printf("%016llX: wrote %u (0x%X) bytes\n", addr, size, size);
    else
      printf("%016llX: failed to write data to process\n", addr);
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // free the data, if allocated
  if (datavalue)
    free(datavalue);
  return 0;
}

// close the current search or a search specified by name
static int command_close(struct state* st, const char* command) {

  // no name present? then we're closing the current search
  if (!command[0]) {

    // check if there's a current search
    if (!st->search) {
      printf("no search currently open\n");
      return 0;
    }

    // delete the search. if it has a name, delete it from the searches list;
    // if not, just delete it directly (unnamed searches shouldn't be added to
    // the searches list)
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

// read registers for all threads in the target process
static int command_read_regs(struct state* st, const char* command) {

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // get registers for target threads
  VMThreadState* thread_state;
  int x, error = VMGetProcessRegisters(st->pid, &thread_state);
  if (error < 0)
    printf("failed to get registers; error %d\n", error);
  else if (error == 0)
    printf("no threads in process\n");
  else {

    // success? then print the regs for each thread
    for (x = 0; x < error; x++)
      VMPrintThreadRegisters(&thread_state[x]);
    free(thread_state);
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  return 0;
}

// set a register value for all threads in the target process
static int command_write_regs(struct state* st, const char* command) {

  // read the value and regname
  uint64_t regvalue = 0;
  sscanf(command, "%llX", &regvalue);
  command = skip_word(command, ' ');

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // read the thread regs on each thread
  VMThreadState* thread_state;
  int x, error = VMGetProcessRegisters(st->pid, &thread_state);
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
      if (st->freeze_while_operating)
        VMResumeProcess(st->pid);
      return 0;
    }

    // write the reg contents back to the process, and print the regs if
    // successful
    error = VMSetProcessRegisters(st->pid, thread_state, error);
    if (error)
      printf("failed to set registers; error %d\n", error);
    else
      for (x = 0; x < error; x++)
        VMPrintThreadRegisters(&thread_state[x]);

    // clean up
    free(thread_state);
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  return 0;
}

// handler for breakpoint exceptions in the target process
// for now, just prints out the breakpoint info and clears dr7
static int _breakpoint_handler(mach_port_t thread, int exception,
                               VMThreadState* state) {

  printf("exception in target process\n");

  const char* type = "unknown";
  if (exception == EXC_BAD_ACCESS)
    type = "bad access";
  if (exception == EXC_BAD_INSTRUCTION)
    type = "bad instruction";
  if (exception == EXC_ARITHMETIC)
    type = "arithmetic";
  if (exception == EXC_EMULATION)
    type = "emulation";
  if (exception == EXC_SOFTWARE)
    type = "software";
  if (exception == EXC_BREAKPOINT)
    type = "breakpoint";
  if (exception == EXC_SYSCALL)
    type = "syscall";
  if (exception == EXC_MACH_SYSCALL)
    type = "mach syscall";
  printf("  type: %s\n", type);

  VMPrintThreadRegisters(state);

  // read the regs for each thread
  VMThreadState* thread_state = NULL;
  int error = VMGetProcessRegisters(st->pid, &thread_state);
  if (error <= 0)
    printf("warning: failed to get target registers\n");
  else {

    // change the breakpoint address register in each thread
    for (x = 0; x < error; x++) {
      if (thread_state[x].is64)
        thread_state[x].db64.__dr7 &= 0xFFFFFFFFFFF0FFFC;
      else
        thread_state[x].db32.__dr7 &= 0xFFF0FFFC;
    }

    // write the reg contents back to the process
    error = VMSetProcessRegisters(st->pid, thread_state, error);
    if (error)
      printf("warning: failed to clear dr7\n");
    free(thread_state);
  }

  return 1; // resume
}

// set a breakpoint on a specific address
static int command_breakpoint(struct state* st, const char* command) {

  // read the type and size of the breakpoint
  uint64_t addr;
  int x, type = 0;
  int size = 0;
  for (; *command && (*command != ' '); command++) {
    if (*command == 'x' || *command == 'X')
      type = 0;
    if (*command == 'w' || *command == 'W')
      type = 1;
    if (*command == 'i' || *command == 'I')
      type = 2;
    if (*command == 'r' || *command == 'R')
      type = 3;
    if (*command == '1')
      size = 0;
    if (*command == '2')
      size = 1;
    if (*command == '4')
      size = 3; // WHY INTEL, WHY?
    if (*command == '8')
      size = 2;
  }

  // read the bp address
  sscanf(command, "%llX", &addr);

  // stop the task
  int paused = VMPauseProcess(st->pid);
  if (!paused)
    printf("warning: failed to stop process while setting breakpoint\n");

  // read the regs for each thread
  VMThreadState* thread_state = NULL;
  int error = VMGetProcessRegisters(st->pid, &thread_state);
  if (error <= 0) {
    printf("failed to get target registers\n");
    goto command_breakpoint_error;
  }

  // change the breakpoint address register in each thread
  for (x = 0; x < error; x++) {
    if (thread_state[x].is64)
      thread_state[x].db64.__dr0 = addr;
    else
      thread_state[x].db32.__dr0 = addr;
  }

  // write the reg contents back to the process
  error = VMSetProcessRegisters(st->pid, thread_state, error);
  if (error) {
    printf("failed to set dr0\n");
    goto command_breakpoint_error;
  }
  free(thread_state);
  thread_state = NULL;

  // read the thread regs on each thread
  error = VMGetProcessRegisters(st->pid, &thread_state);
  if (error <= 0) {
    printf("failed to get dr7\n");
    goto command_breakpoint_error;
  }

  // change the breakpoint control reg in each thread
  // DR7 format: 33332222 11110000 -------- 33221100
  //             ^ sizetype                 ^ enable
  // sizetype = SSTT
  // SS = 00: 1 byte, 01: 2 bytes, 10: 8 bytes, 11: 4 bytes
  // TT = 00: exec, 01: write, 10: io, 11: rw
  for (x = 0; x < error; x++) {
    if (thread_state[x].is64) {
      thread_state[x].db64.__dr7 &= 0xFFFFFFFFFFF0FFFC;
      thread_state[x].db64.__dr7 |= (1 | (type << 16) | (size << 18));
    } else {
      thread_state[x].db32.__dr7 &= 0xFFF0FFFC;
      thread_state[x].db32.__dr7 |= (1 | (type << 16) | (size << 18));
    }
  }

  // write the reg contents back to the process
  error = VMSetProcessRegisters(st->pid, thread_state, error);
  if (error) {
    printf("failed to set dr7\n");
    goto command_breakpoint_error;
  }

  printf("waiting for breakpoint\n");

  // wait for the breakpoint
  error = VMWaitForBreakpoint(st->pid, _breakpoint_handler);
  if (error) {
    printf("failed to wait for breakpoint\n");
    goto command_breakpoint_error;
  }

  // clean up
  if (0) {
command_breakpoint_error:
    printf("failed to set breakpoint; error %d\n", error);
  }
  if (thread_state)
    free(thread_state);

  // resume process if it was originally paused
  if (paused)
    VMResumeProcess(st->pid);
  return 0;
}

// pause the target process
static int command_pause(struct state* st, const char* command) {
  if (VMPauseProcess(st->pid))
    printf("process suspended\n");
  else
    printf("failed to pause process\n");
  return 0;
}

// resume the target process
static int command_resume(struct state* st, const char* command) {
  if (VMResumeProcess(st->pid))
    printf("process resumed\n");
  else
    printf("failed to resume process\n");
  return 0;
}

// terminate the target process
static int command_terminate(struct state* st, const char* command) {
  if (VMTerminateProcess(st->pid))
    printf("process terminated\n");
  else
    printf("failed to terminate process\n");
  return 0;
}

// send a signal the target process
static int command_signal(struct state* st, const char* command) {
  int sig = atoi(command);
  kill(st->pid, sig);
  if (sig == SIGKILL)
    st->run = 0;
  printf("signal %d sent to process\n", sig);
  return 0;
}

// attach to a different process
static int command_attach(struct state* st, const char* command) {

  // if no command given, use the current process name
  // (i.e. if program was quit & restarted, its pid will probably change)
  if (!command[0])
    command = st->processname;

  // try to read new pid
  pid_t new_pid = atoi(command);
  if (!new_pid) {

    // no number? then it's a process name
    int num_results = getnamepid(command, &new_pid, 0);
    if (num_results == 0) {
      printf("warning: no processes found by name; searching commands\n");
      num_results = getnamepid(command, &new_pid, 1);
    }
    if (num_results == 0) {
      printf("error: no processes found\n");
      return 0;
    }
    if (num_results > 1) {
      printf("error: multiple processes found\n");
      return 0;
    }
  }

  // if we can get its name, we're fine - the process exists
  char new_processname[PROCESS_NAME_LENGTH] = "";
  getpidname(new_pid, new_processname, PROCESS_NAME_LENGTH);
  if (new_processname[0]) {
    st->pid = new_pid;
    strcpy(st->processname, new_processname);
    MoveFrozenRegionsToProcess(st->pid);

    // warn user if pid is memwatch itself
    if (st->pid == getpid())
      printf("warning: memwatch is operating on itself\n");
    printf("switched to new process %u/%s\n", st->pid, st->processname);
  }
  return 0;
}

// exit memwatch
static int command_quit(struct state* st, const char* command) {
  st->run = 0;
  return 0;
}



// list of pointers to functions for each command
typedef int (*command_handler)(struct state* st, const char* command);

static const struct {
  const char* word;
  command_handler func;
} command_handlers[] = {
  {"access", command_access},
  {"attach", command_attach},
  {"at", command_attach},
  {"a", command_access},
  {"break", command_breakpoint},
  {"b", command_breakpoint},
  {"close", command_close},
  {"c", command_close},
  {"delete", command_delete},
  {"del", command_delete},
  {"dump", command_dump},
  {"d", command_dump},
  {"find", command_find},
  {"fi", command_find},
  {"freeze", command_freeze},
  {"fr", command_freeze},
  {"f", command_freeze},
  {"help", command_help},
  {"h", command_help},
  {"kill", command_terminate},
  {"k", command_terminate},
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
  {"set", command_set},
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
int prompt_for_commands(pid_t pid, int freeze_while_operating) {

  // construct the initial state
  struct state st;
  memset(&st, 0, sizeof(struct state));
  st.pid = pid;
  st.run = 1;
  st.searches = CreateSearchList();
  st.freeze_while_operating = freeze_while_operating;

  // get the process name
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

    // decide what to prompt the user with (include the seearch name if a search
    // is currently open)
    // prompt is (memwatch:<pid>/<processname>#<searchname>)
    // or (memwatch:<pid>/<processname>)
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

    // read a command and add it to the command history
    command = readline(prompt);
    trim_spaces(command);
    if (command && command[0])
      add_history(command);
    free(prompt);

    // find the entry in the command table and run the command
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
