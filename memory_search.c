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

// memory searching user interface!
int memory_search(pid_t pid, int pause_during) {

  // first get the process name
  char processname[PROCESS_NAME_LENGTH];
  if (!pid)
    strcpy(processname, "KERNEL");
  else
    getpidname(pid, processname, PROCESS_NAME_LENGTH);

  // no process name? process doesn't exist!
  if (!strlen(processname)) {
    printf("process %u does not exist\n", pid);
    return (-2);
  }

  // init the region freezer
  InitRegionFreezer();

  // yep, lots of variables (see the code below for their explanations)
  int process_running = 1, cont;
  char *command = NULL;
  const char *command_read = NULL, *arguments = NULL;
  int x, error, run = 1;
  unsigned long long size;
  unsigned long long addr;
  void* write_data = NULL;
  VMRegionDataMap* map = NULL;
  VMThreadState* thread_state = NULL;
  MemorySearchDataList* searches = CreateSearchList();
  MemorySearchData* search = NULL; // the current search

  // while we have stuff to do...
  while (run) {

    // decide what to prompt the user with
    char* prompt;
    if (search) {
      prompt = (char*)malloc(30 + strlen(processname) +
                                   strlen(search->name));
      sprintf(prompt, "(memwatch:%u/%s#%s) ", pid, processname, search->name);
    } else {
      prompt = (char*)malloc(30 + strlen(processname));
      sprintf(prompt, "(memwatch:%u/%s) ", pid, processname);
    }

    // delete the old command, if present
    if (command)
      free(command);
    command = readline(prompt);
    trim_spaces(command);
    if (command && command[0])
      add_history(command);
    free(prompt);

    // what is thy bidding, my master?
    switch (command[0]) {

      // print help message
      case 'h':
      case 'H':
        printf(
"memwatch memory search utility\n"
"\n"
"commands:\n"
"  l                         list memory regions\n"
"  d                         dump memory\n"
"  d <filename>              dump memory regions to <filename>_<addr> files\n"
"  t <data>                  find occurrences of data\n"
"  m <addr>                  enable all access on region containing address\n"
"  r <addr+size>             read from memory\n"
"  R <addr+size>             read from memory every second (ctrl+c to stop)\n"
"  w <addr> <data>           write to memory\n"
"  W <addr> <filename>       write file to memory\n"
"  f <addr> <data>           freeze data in memory\n"
"  f \"<name>\" <addr> <data>  freeze data in memory, with given name\n"
"  u                         list frozen regions\n"
"  u <index>                 unfreeze frozen region by index\n"
"  u <address>               unfreeze frozen region by address\n"
"  u <name>                  unfreeze frozen regions by name\n"
"  S                         show previous named searches\n"
"  S <name>                  switch to a previous named search\n"
"  S <type> [name]           begin new search over writable memory only\n"
"  S! <type> [name]          begin new search over all memory\n"
"  s <operator> [value]      search for a changed value\n"
"  x                         print current search results\n"
"  p                         delete current search\n"
"  p <name>                  delete search by name\n"
"  g                         view register contents on all threads\n"
"  G <value> <reg>           modify register contents on all threads\n"
"  -                         pause process\n"
"  +                         resume process\n"
"  * <signal_number>         send unix signal to process\n"
"  q                         exit memwatch\n"
"\n"
"<addr+size> may be either <addr(hex)> <size(dec)> or <addr(hex)>:<size(hex)>.\n"
"all <data> arguments are in immediate format (see main usage statement).\n"
"\n"
"the f command is like the w command, but the write is repeated in the\n"
"  background until canceled by a u command.\n"
"new freezes will be named with the same name as the current search (if any),\n"
"  unless a specific name is given\n"
"\n"
"searches done with the s command will increment on the previous search and\n"
"  narrow down the results. searches done with the t command are one-time\n"
"  searches and don\'t affect the current search results.\n"
"note that a search name is optional: if no name is given, the search is deleted\n"
"  upon switching to a named search or opening a new search.\n"
"\n"
"valid types for S command: u8, u16, u32, u64, s8, s16, s32, s64,\n"
"  float, double, arbitrary.\n"
"any type except arbitrary may be prefixed with l to make it reverse-endian.\n"
"arbitrary may be used to search for a string; i.e. the values being searched\n"
"  for are given in immediate data format.\n"
"reverse-endian searches are usually not necessary. one case in which they may\n"
"  be necessary is when the target is an emulator emulating a reverse-endian\n"
"  system; i.e. on mac os x, a powerpc app will require reverse-endian searches.\n"
"\n"
"valid operators for s command: = < > <= >= != $\n"
"the $ operator does a flag search (finds values that differ by only one bit).\n"
"  for example, 04 $ 0C is true, but 04 $ 02 is false.\n"
"if the value on the s command is omitted, the previous value (from the last\n"
"  search) is used.\n"
"\n"
"example: find all occurrences of the string \"the quick brown fox\"\n"
"  t \"the quick brown fox\"\n"
"example: read 0x100 bytes from 0x00002F00\n"
"  r 2F00:100\n"
"example: write \"the quick brown fox\" and terminating \\0 to 0x2F00\n"
"  w 2F00 \"the quick brown fox\"00\n"
"example: open a search named \"score\" for a 16-bit signed int\n"
"  S s16 score\n"
"example: search for the value 2160\n"
"  s = 2160\n"
"example: search for values less than 30000\n"
"  s < 30000\n"
"example: search for values greater than or equal to the previous value\n"
"  s >=\n"
               );
        break;

      // list memory regions
      case 'l':
      case 'L':

        // stop the process if necessary, get the list, and resume the process
        if (process_running && pause_during)
          kill(pid, SIGSTOP);
        map = GetProcessRegionList(pid, 0);
        if (process_running && pause_during)
          kill(pid, SIGCONT);
        if (!map) {
          printf("get process region list failed\n");
          break;
        }

        // print it out and delete it
        print_region_map(map);
        DestroyDataMap(map);

        break;

      // dump memory
      case 'd':
      case 'D':

        // read the filename prefix
        arguments = skip_word(command, ' ');

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          kill(pid, SIGSTOP);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          kill(pid, SIGCONT);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // save each piece with the given filename
        if (arguments[0]) {
          char* filename_piece = (char*)malloc(strlen(arguments) + 64);

          unsigned long x;
          for (x = 0; x < map->numRegions; x++) {

            sprintf(filename_piece, "%s_%016llX", arguments,
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
        break;

      // find a string in memory
      case 't':
      case 'T':

        // stop the process if necessary, dump memory, and resume the process
        printf("reading memory\n");
        if (process_running && pause_during)
          kill(pid, SIGSTOP);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          kill(pid, SIGCONT);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // read the string
        size = read_string_data(skip_word(command, ' '), &write_data);

        // find the string in a region somewhere
        unsigned long long num_results = 0;
        cont = 1;
        cancel_var = &cont;
        for (x = 0; cont && (x < map->numRegions); x++) {

          // skip regions with no data
          if (!map->regions[x].data)
            continue;

          int y;
          for (y = 0; y <= map->regions[x].region._size - size; y++) {
            if (!memcmp(&map->regions[x].s8_data[y], write_data, size)) {
              printf("string found at %016llX\n",
                     map->regions[x].region._address + y);
              num_results++;
            }
          }
        }
        cancel_var = NULL;
        printf("results: %llu\n", num_results);

        free(write_data);
        DestroyDataMap(map);
        break;

      // make a region all-access
      case 'm':
      case 'M':

        // read the address
        sscanf(skip_word(command, ' '), "%llX", &addr);

        // attempt to make it all-access
        if (VMSetRegionProtection(pid, addr, 1, VMREGION_ALL, VMREGION_ALL))
          printf("region protection set to all access\n");
        else
          printf("failed to change region protection\n");
        break;

      // read from memory
      case 'r':
      case 'R':

        // read addr/size
        if (!read_addr_size(skip_word(command, ' '), &addr, &size)) {
          printf("invalid command format\n");
          break;
        }

        // alloc local read buffer
        void* read_data = malloc(size);
        if (!read_data) {
          printf("failed to allocate memory for reading\n");
          break;
        }

        // if it's lowercase, do it once; if it's uppercase, do it once every
        // second
        cont = (command[0] == 'R');
        cancel_var = &cont;
        do {
          if ((error = VMReadBytes(pid, addr, read_data, &size)))
            print_process_data(processname, addr, read_data, size);
          else
            printf("failed to read data from process\n");
          if (cont)
            usleep(1000000);
        } while (cont);
        free(read_data);
        cancel_var = NULL;
        break;

      // write data to memory
      case 'w':

        // read the address and data
        command_read = skip_word(command, ' ');
        sscanf(command_read, "%llX", &addr);
        command_read = skip_word(command_read, ' ');
        size = read_string_data(command_read, &write_data);

        // and write it
        if ((error = VMWriteBytes(pid, addr, write_data, size)))
          printf("wrote %llu (0x%llX) bytes\n", size, size);
        else
          printf("failed to write data to process\n");
        free(write_data);
        break;

      // write file to memory
      case 'W':

        // read the parameters
        command_read = skip_word(command, ' ');
        sscanf(command_read, "%llX", &addr);
        command_read = skip_word(command_read, ' '); // skip space after addr/size

        // write the file
        write_file_to_process(command_read, size, pid, addr);

        break;

      // freeze value with immediate data
      case 'f':

        // if a quote is given, read the name
        command_read = skip_word(command, ' ');
        char* freeze_name = NULL;
        if ((command_read[0] == '\'') || (command_read[0] == '\"')) {
          command_read += copy_quoted_string(command_read, &freeze_name);
          command_read = skip_word(command_read, ' ');
        }

        // read the address
        sscanf(command_read, "%llX", &addr);
        command_read = skip_word(command_read, ' ');

        // read the data
        size = read_string_data(command_read, &write_data);

        // and freeze it
        char* use_name = freeze_name ? freeze_name :
                         (search ? search->name : "[no associated search]");
        if (FreezeRegion(pid, addr, size, write_data, use_name))
          printf("failed to freeze region\n");
        else
          printf("region frozen\n");
        free(write_data);
        if (freeze_name)
          free(freeze_name);
        break;

      // unfreeze a var by index, or print frozen regions
      case 'u':

        // find the unfreeze index
        command_read = skip_word(command, ' ');

        // index given? unfreeze it
        if (command_read[0]) {
          int num_by_name = UnfreezeRegionByName(command_read);
          if (num_by_name == 1) {
            printf("region unfrozen\n");
          } else if (num_by_name > 1) {
            printf("%d regions unfrozen\n", num_by_name);
          } else {
            sscanf(command_read, "%llX", &addr);
            if (!UnfreezeRegionByAddr(addr))
              printf("region unfrozen\n");
            else {
              sscanf(command_read, "%llu", &addr);
              if (!UnfreezeRegionByIndex(addr))
                printf("region unfrozen\n");
              else
                printf("failed to unfreeze region\n");
            }
          }

        // else, print frozen regions
        } else
          PrintFrozenRegions(0);

        break;

      // begin new search
      case 'S':

        // read the type. if no type, show the list of searches
        arguments = skip_word(command, ' ');
        if (!arguments[0]) {
          PrintSearches(searches);
          break;
        }

        // check if a search by this name exists or not
        search = GetSearchByName(searches, arguments);
        if (search) {
          printf("switched to search %s\n", search->name);
          break;
        }

        // check if a name followed the type
        const char* name = skip_word(arguments, ' ');

        // if it's S!, we'll search read-only as well (don't know why anyone
        // would want to do this though... :/)
        int search_flags = (command[1] == '!') ? SEARCHFLAG_ALLMEMORY : 0;
        
        // make a new search
        search = CreateNewSearchByTypeName(arguments, name, search_flags);
        if (search) {
          AddSearchToList(searches, search);
          if (search->name[0])
            printf("opened new %s search named %s\n",
                   GetSearchTypeName(search->type), name);
          else
            printf("opened new %s search (unnamed)\n",
                   GetSearchTypeName(search->type));
        } else
          printf("failed to open new search - did you use a valid typename?\n");

        break;

      // search for a value
      case 's':

        // make sure a search is opened
        if (!search) {
          printf("no search is currently open; use the S command to open a "
                 "search\n");
          break;
        }

        // read the predicate
        arguments = skip_word(command, ' ');
        int pred = GetPredicateByName(arguments);

        // check if a value followed the predicate
        void* value = NULL;
        size = 0;
        write_data = NULL;
        const char* value_text = skip_word(arguments, ' ');
        if (*value_text) {

          // we have a value... blargh
          unsigned long long ivalue;
          float fvalue;
          double dvalue;

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
            size = read_string_data(value_text, &write_data);
            value = write_data;
          }
        }

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          kill(pid, SIGSTOP);
        map = DumpProcessMemory(pid, search->searchflags & SEARCHFLAG_ALLMEMORY
                                ? 0 : VMREGION_WRITABLE);
        if (process_running && pause_during)
          kill(pid, SIGCONT);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // run the search
        MemorySearchData* search_result = ApplyMapToSearch(search, map, pred,
                                                           value, size);
        map = NULL;
        if (write_data)
          free(write_data);

        // success? then save the result
        if (!search_result) {
          printf("search failed\n");
          break;
        }

        // add this search to the list
        search = search_result;
        AddSearchToList(searches, search);
        if (search->numResults >= 20) {
          printf("results: %lld\n", search->numResults);
          break;
        }

        // fall through to display results if there are less than 20

      // print search results
      case 'x':
      case 'X':
        if (!search) {
          printf("no search currently open\n");
          break;
        }
        if (!search->memory) {
          printf("no initial search performed\n");
          break;
        }
        for (x = 0; x < search->numResults; x++)
          printf("%016llX\n", search->results[x]);
        printf("results: %lld\n", search->numResults);
        
        break;

      // delete current search
      case 'p':
      case 'P':

        // read the type
        arguments = skip_word(command, ' ');

        // no name present? then we're deleting the current search
        if (!arguments[0]) {

          // check if there's a current search
          if (!search) {
            printf("no search currently open\n");
            break;
          }

          // delete the search
          if (search->name[0])
            DeleteSearchByName(searches, search->name);
          else
            DeleteSearch(search);
          search = NULL;

        // name present? then delete the search by name
        } else {

          // if the search we're deleting is the current search, then the
          // current search should become NULL
          if (search && !strcmp(arguments, search->name))
            search = NULL;

          // delete the search
          DeleteSearchByName(searches, arguments);
        }

        break;

      // view registers on threads
      case 'g':
        error = VMGetThreadRegisters(pid, &thread_state);
        if (error < 0)
          printf("failed to get registers; error %d\n", error);
        else if (error == 0)
          printf("no threads in process\n");
        else {
          for (x = 0; x < error; x++)
            VMPrintThreadRegisters(&thread_state[x]);
          free(thread_state);
        }
        break;

      // set register on threads
      case 'G':

        // read the value and regname
        arguments = skip_word(command, ' ');
        uint64_t regvalue = 0;
        sscanf(arguments, "%llX", &regvalue);
        arguments = skip_word(arguments, ' ');

        // read the thread regs on each thread
        error = VMGetThreadRegisters(pid, &thread_state);
        if (error < 0)
          printf("failed to get registers; error %d\n", error);
        else if (error == 0)
          printf("no threads in process\n");
        else {

          // change the reg in each thread
          for (x = 0; x < error; x++)
            if (VMSetRegisterValueByName(&thread_state[x], arguments, regvalue))
              break;
          if (x < error) {
            free(thread_state);
            printf("invalid register name\n");
            break;
          }

          // print regs to write
          for (x = 0; x < error; x++)
            VMPrintThreadRegisters(&thread_state[x]);

          // write the reg contents back to the process
          error = VMSetThreadRegisters(pid, thread_state, error);
          if (error)
            printf("failed to set registers; error %d\n", error);
          else
            printf("modified thread registers\n");
          free(thread_state);
        }

        break;

      // set breakpoint in dr0
      case 'B':

        // read the value and regname
        size = 0;
        for (arguments = skip_word(command, ' ');
             *arguments && (*arguments != ' '); arguments++) {
          if (*arguments == 'x' || *arguments == 'X')
            size = 0;
          if (*arguments == 'w' || *arguments == 'W')
            size = 1;
          if (*arguments == 'i' || *arguments == 'I')
            size = 2;
          if (*arguments == 'r' || *arguments == 'R')
            size = 3;
        }
        sscanf(arguments, "%llX", &addr);

        // read the thread regs on each thread
        error = VMGetThreadRegisters(pid, &thread_state);
        if (error < 0)
          printf("failed to get registers; error %d\n", error);
        else if (error == 0)
          printf("no threads in process\n");
        else {
          
          // change the reg in each thread
          for (x = 0; x < error; x++) {
            if (thread_state[x].is64) {
              thread_state[x].db64.__dr0 = addr;
              thread_state[x].db64.__dr7 |= (0x000C0001 | (size << 16));
            } else {
              thread_state[x].db32.__dr0 = addr;
              thread_state[x].db32.__dr7 |= (0x000C0001 | (size << 16));
            }
          }

          // print regs to write
          for (x = 0; x < error; x++)
            VMPrintThreadRegisters(&thread_state[x]);

          // write the reg contents back to the process
          error = VMSetThreadRegisters(pid, thread_state, error);
          if (error)
            printf("failed to set registers; error %d\n", error);
          else
            printf("modified thread registers\n");
          free(thread_state);
        }

        break;

      // pause process
      case '-':
        kill(pid, SIGSTOP);
        process_running = 0;
        printf("process suspended\n");
        break;

      // resume process
      case '+':
        kill(pid, SIGCONT);
        process_running = 1;
        printf("process resumed\n");
        break;

      // send signal to process
      case '*':
        size = atoi(skip_word(command, ' '));
        kill(pid, size);
        if (size == SIGSTOP)
          process_running = 0;
        if (size == SIGCONT)
          process_running = 1;
        if (size == SIGKILL)
          run = 0;
        printf("signal %llu sent to process\n", size);
        break;

      // quit
      case 'q':
      case 'Q':
        run = 0;
        break;

      // unknown
      default:
        printf("unknown command - try \'h\'\n");
    }
  }

  // shut down the region freezer and return
  DeleteSearchList(searches);
  CleanupRegionFreezer();
  return 0;
}
