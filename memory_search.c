// memory_search.c, by Martin Michelsen, 2012
// interface for interactive memory search

#include <readline/readline.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "freeze_region.h"
#include "memory_search.h"
#include "parse_utils.h"
#include "process_utils.h"
#include "search_data.h"
#include "search_data_list.h"
#include "vmmap.h"
#include "vmmap_data.h"

// TODO: undoing searches

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
  // this is cheating but it means I don't have to maintain two very similar
  // sets of documentation
  int retcode = system("man memwatch");
  if (retcode)
    printf("failed to open the man page - memwatch may not be installed properly on your system\n");
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
        printf("%016llX (%c%c%c)\n", map->regions[x].region._address + y,
            (map->regions[x].region._attributes & VMREGION_READABLE) ? 'r' : '-',
            (map->regions[x].region._attributes & VMREGION_WRITABLE) ? 'w' : '-',
            (map->regions[x].region._attributes & VMREGION_EXECUTABLE) ? 'x' : '-');
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
  int mode;
  uint64_t addr;
  sscanf(command, "%d %llX", &mode, &addr);

  // attempt to change the access permissions
  int err = VMSetRegionProtection(st->pid, addr, 1, mode, VMREGION_ALL);
  if (!err)
    printf("region protection changed\n");
  else
    printf("failed to change region protection (error %d)\n", err);

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
  int addrsize_read_result = read_addr_size(command, &addr, &size);
  if (!addrsize_read_result) {
    printf("invalid command format\n");
    return 0;
  }
  command += addrsize_read_result;

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

    error = VMReadBytes(st->pid, addr, read_data, &size);
    if (error)
      printf("failed to read data from process (error %d)\n", error);
    else {
      if (command[0]) {
        FILE* f = fopen(command, "wb");
        if (f) {
          printf("wrote %lu bytes to file\n", fwrite(read_data, 1, size, f));
          fclose(f);
        } else
          printf("failed to open file\n");
      } else {
        print_process_data(st->processname, addr, read_data,
                           times ? read_data_prev : NULL, size);
      }
    }

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
    VMPauseProcess(st->pid);

  // write the data to the target's memory
  int error = VMWriteBytes(st->pid, addr, data, size);
  if (!error)
    printf("wrote %llu (0x%llX) bytes\n", size, size);
  else
    printf("failed to write data to process (error %d)\n", error);

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // clean up & return
  free(data);
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
  void* data;
  uint64_t addr, size;
  int num_args = sscanf(command, "%llX:%llX", &addr, &size);
  if (num_args == 2) {

    // user gave a size... read the data to be frozen from the process
    data = malloc(size);
    if (!data) {
      printf("failed to allocate memory for reading\n");
      return 0;
    }

    if (st->freeze_while_operating)
      VMPauseProcess(st->pid);
    int error = VMReadBytes(st->pid, addr, data, &size);
    if (st->freeze_while_operating)
      VMResumeProcess(st->pid);

    if (!error)
      printf("read %llu (0x%llX) bytes\n", size, size);
    else {
      printf("failed to read data from process (error %d)\n", error);
      free(data);
      return 0;
    }

  } else {
    // read the data
    command = skip_word(command, ' ');
    size = read_string_data(command, &data);
  }

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

// prints a search's result set
static void print_search_results(struct state* st, MemorySearchData* search,
    int max_results) {

  // if there's a max given, don't print results if there are more than that
  // many of them
  if (max_results && (search->numResults > max_results)) {
    printf("results: %lld\n", search->numResults);
    return;
  }

  // print the results!
  int x;
  if (search->type == SEARCHTYPE_DATA) {
    for (x = 0; x < search->numResults; x++)
      printf("%016llX\n", search->results[x]);
  } else {
    mach_vm_size_t size = SearchDataSize(search->type);
    void* data = malloc(size);
    for (x = 0; x < search->numResults; x++) {
      printf("%016llX ", search->results[x]);
      int error = VMReadBytes(st->pid, search->results[x], data, &size);
      if (error)
        printf("<< memory not readable, %d >>\n", error);
      else {
        if (IsReverseEndianSearchType(search->type))
          bswap(data, size);
        switch (search->type) {
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
  printf("results: %lld\n", search->numResults);
}

// print list of results for current search, or the named search
static int command_results(struct state* st, const char* command) {

  // if a search name is given, show results for that search
  MemorySearchData* search = st->search;
  if (command && command[0])
    search = GetSearchByName(st->searches, command);

  // can't do this if there isn't a current search, or if the current search
  // object has no memory associated with it (and therefore no valid results)
  if (!search) {
    printf("search not found, or no search currently open\n");
    return 0;
  }
  if (!search->memory) {
    printf("no initial search performed; results not available\n");
    return 0;
  }

  print_search_results(st, search, 0);
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
  print_search_results(st, st->search, 20);
  return 0;
}

// execute a search using the current search object
static int command_search(struct state* st, const char* command) {

  MemorySearchData* search = st->search;

  // read the predicate
  int pred = GetPredicateByName(command);

  // if the command doesn't begin with a valid predicate, it may be of the form
  // "search search_name predicate [value]"
  if (pred == PRED_UNKNOWN) {
    char* search_name = get_word(command, ' ');
    if (!search_name) {
      printf("invalid predicate type\n");
      return 0;
    }
    search = GetSearchByName(st->searches, search_name);
    free(search_name);

    command = skip_word(command, ' ');
    pred = GetPredicateByName(command);
  }

  // if there's no search, then we can't do anything
  if (!search) {
    printf("no search is open and no search was specified by name\n");
    return 0;
  }

  // check if a value followed the predicate
  void *value = NULL, *data = NULL;
  unsigned long long ivalue;
  float fvalue;
  double dvalue;
  uint64_t size = 0;
  const char* value_text = skip_word(command, ' ');
  if (*value_text) {

    // we have a value... read it in
    if (IsIntegerSearchType(search->type)) {
      sscanf(value_text, "%lld", &ivalue);
      value = &ivalue;
    } else if (search->type == SEARCHTYPE_FLOAT ||
               search->type == SEARCHTYPE_FLOAT_LE) {
      sscanf(value_text, "%f", &fvalue);
      value = &fvalue;
    } else if (search->type == SEARCHTYPE_DOUBLE ||
               search->type == SEARCHTYPE_DOUBLE_LE) {
      sscanf(value_text, "%lf", &dvalue);
      value = &dvalue;
    } else if (search->type == SEARCHTYPE_DATA) {
      size = read_string_data(value_text, &data);
      value = data;
    }
  }

  if (st->freeze_while_operating)
    VMPauseProcess(st->pid);

  // take a snapshot of the target's memory
  VMRegionDataMap* map = DumpProcessMemory(st->pid,
      search->searchflags & SEARCHFLAG_ALLMEMORY ? 0 : VMREGION_WRITABLE);
  if (!map) {
    printf("memory dump failed\n");
    return 0;
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  // run the search, then delete the data (if allocated)
  MemorySearchData* search_result = ApplyMapToSearch(search, map, pred, value,
      size);
  if (data)
    free(data);

  // success? then save the result
  if (!search_result) {
    printf("search failed\n");
    return 0;
  }

  // add this search to the list. don't need to delete the old one here - it'll
  // be deleted by AddSearchToList if it gets replaced by the new one
  if (search == st->search)
    st->search = search_result;
  AddSearchToList(st->searches, search_result);

  // print results if there aren't too many of them
  print_search_results(st, search_result, 20);
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
    VMPauseProcess(st->pid);

  // write the data to each result address
  int x, error;
  for (x = 0; x < st->search->numResults; x++) {
    uint64_t addr = st->search->results[x];
    error = VMWriteBytes(st->pid, addr, data, size);
    if (!error)
      printf("%016llX: wrote %u (0x%X) bytes\n", addr, size, size);
    else
      printf("%016llX: failed to write data to process (error %d)\n", addr,
             error);
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
      VMPrintThreadRegisters(&thread_state[x], x);
    free(thread_state);
  }

  if (st->freeze_while_operating)
    VMResumeProcess(st->pid);

  return 0;
}

// set a register value for all threads in the target process
static int command_write_regs(struct state* st, const char* command) {

  // read the value and regname
  const char* regname = command;
  command = skip_word(command, ' ');
  uint64_t regvalue = 0;
  sscanf(command, "%llX", &regvalue);
  command = skip_word(command, ' ');
  int thread_id = command[0] ? atoi(command) : -1;

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
    for (x = 0; x < error; x++) {
      // if a thread_id is given, only write to that thread's regs
      if (thread_id >= 0 && x != thread_id)
        continue;
      if (VMSetRegisterValueByName(&thread_state[x], regname, regvalue))
        break;
    }
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
        VMPrintThreadRegisters(&thread_state[x], x);

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

  VMPrintThreadRegisters(state, -1);

  // read the regs for each thread
  /*VMThreadState* thread_state = NULL;
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
  } */

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
  int error = VMPauseProcess(st->pid);
  if (!error)
    printf("process suspended\n");
  else
    printf("failed to pause process (error %d)\n", error);
  return 0;
}

// resume the target process
static int command_resume(struct state* st, const char* command) {
  int error = VMResumeProcess(st->pid);
  if (!error)
    printf("process resumed\n");
  else
    printf("failed to resume process (error %d)\n", error);
  return 0;
}

// terminate the target process
static int command_terminate(struct state* st, const char* command) {
  int error = VMTerminateProcess(st->pid);
  if (!error)
    printf("process terminated\n");
  else
    printf("failed to terminate process (error %d)\n", error);
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
  {"t", command_find},
  {"unfreeze", command_unfreeze},
  {"u", command_unfreeze},
  {"wregs", command_write_regs},
  {"write", command_write},
  {"wr", command_write},
  {"w", command_write},
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
    // prompt is memwatch:<pid>/<processname> <num_search>/<num_frozen> <searchname> #
    // or memwatch:<pid>/<processname> <num_search>/<num_frozen> #
    char* prompt;
    if (st.search) {
      prompt = (char*)malloc(50 + strlen(st.processname) +
                             strlen(st.search->name));
      if (!st.search->memory)
        sprintf(prompt, "memwatch:%u/%s %ds/%df %s # ", st.pid, st.processname,
                st.searches->numSearches, GetNumFrozenRegions(),
                st.search->name);
      else
        sprintf(prompt, "memwatch:%u/%s %ds/%df %s(%llu) # ", st.pid,
                st.processname, st.searches->numSearches, GetNumFrozenRegions(),
                st.search->name, st.search->numResults);
    } else {
      prompt = (char*)malloc(30 + strlen(st.processname));
      sprintf(prompt, "memwatch:%u/%s %ds/%df # ", st.pid, st.processname,
              st.searches->numSearches, GetNumFrozenRegions());
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

    // command can be NULL if ctrl+d was pressed - just exit in that case
    if (!command) {
      printf(" -- exit\n");
      break;
    }

    // if no command, do nothing
    if (!command[0]) {
      continue;
    }

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
