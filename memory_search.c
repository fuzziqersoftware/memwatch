#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vmmap.h"
#include "vmmap_data.h"
#include "search_data.h"
#include "search_data_list.h"

#include "parse_utils.h"
#include "process_utils.h"
#include "freeze_region.h"

#include "memory_search.h"

#define read_addr_size(addr, size) \
  { fscanf(stdin, "%llX", &addr); \
  command = fgetc(stdin); \
  if (command == ':') \
    fscanf(stdin, "%llX", &size); \
  else if (command == ' ') \
    fscanf(stdin, "%llu", &size); \
  else if (command == '\n') { \
    printf("invalid command format\n"); \
    ungetc('\n', stdin); \
    break; \
  } else { \
    printf("invalid command format\n"); \
    break; \
  } }

extern int* cancel_var;

// memory searching user interface!
int memory_search(pid_t pid, int pause_during) {

  // first get the process name
  char processname[PROCESS_NAME_LENGTH];
  getpidname(pid, processname, PROCESS_NAME_LENGTH);

  // no process name? process doesn't exist!
  if (!strlen(processname)) {
    printf("process %u does not exist\n", pid);
    return (-2);
  }

  // init the region freezer
  InitRegionFreezer();
 
  // yep, lots of variables (see the code below for their explanations)
  int process_running = 1;
  int command = 1;
  char *arguments, *filename;
  int x, error;
  unsigned long long size;
  unsigned long long addr;
  void* write_data = NULL;
  VMRegionDataMap* map = NULL;
  MemorySearchDataList* searches = CreateSearchList();
  MemorySearchData* search = NULL; // the current search

  // while we have stuff to do...
  while (command) {

    // prompt the user
    if (search)
      printf("(memwatch:%u/%s#%s) ", pid, processname, search->name);
    else
      printf("(memwatch:%u/%s) ", pid, processname);

    // what is thy bidding, my master?
    command = fgetc(stdin);
    switch (command) {

      // TODO: print help message
      case 'h':
      case 'H':
        printf(
"memwatch memory search utility\n"
"\n"
"commands:\n"
"  l                         list memory regions\n"
"  d                         dump memory\n"
"  t <data>                  find occurrences of data\n"
"  m <addr>                  enable all access on region containing address\n"
"  r <addr+size>             read from memory\n"
"  w <addr> <data>           write to memory\n"
"  W <addr> <filename>       write file to memory\n"
"  f <addr> <data>           freeze data in memory\n"
"  f \"<name>\" <addr> <data>  freeze data in memory, with given name"
"  u                         list frozen regions\n"
"  u <index>                 unfreeze frozen region\n"
"  S                         show previous named searches\n"
"  S <name>                  switch to a previous named search\n"
"  S <type> [name]           begin new search\n"
"  s <operator> [value]      search for a changed value\n"
"  x                         print current search results\n"
"  p                         delete current search\n"
"  p <name>                  delete search by name\n"
"  -                         pause process\n"
"  +                         resume process\n"
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
          VMStopProcess(pid);
        map = GetProcessRegionList(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
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
        filename = read_string_delimited(stdin, '\n', 0);
        trim_spaces(filename);

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // save each piece with the given filename
        if (filename[0]) {
          char* filename_piece = (char*)malloc(strlen(filename) + 64);

          unsigned long x;
          for (x = 0; x < map->numRegions; x++) {

            sprintf(filename_piece, "%s_%016llX", filename, map->regions[x].region._address);

            // print region info
            printf("%016llX %016llX %016llX %c%c%c%s\n",
                   map->regions[x].region._address,
                   map->regions[x].region._address + map->regions[x].region._size,
                   map->regions[x].region._size,
                   (map->regions[x].region._attributes & VMREGION_READABLE) ? 'r' : '-',
                   (map->regions[x].region._attributes & VMREGION_WRITABLE) ? 'w' : '-',
                   (map->regions[x].region._attributes & VMREGION_EXECUTABLE) ? 'x' : '-',
                   map->regions[x].data ? filename_piece : " [data not read]");

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
        free(filename);

        // delete the data map
        DestroyDataMap(map);
        break;

      // find a string in memory
      case 't':
      case 'T':

        // stop the process if necessary, dump memory, and resume the process
        printf("reading memory\n");
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // read the string
        size = read_stream_data(stdin, &write_data);

        // find the string in a region somewhere
        unsigned long long num_results = 0;
        int cont = 1;
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
        fscanf(stdin, "%llX", &addr);

        // attempt to make it all-access
        if (VMSetRegionProtection(pid, addr, 1, VMREGION_ALL, VMREGION_ALL))
          printf("region protection set to all access\n");
        else
          printf("failed to change region protection\n");
        break;

      // read from memory
      case 'r':
      case 'R':
        read_addr_size(addr, size);

        void* read_data = malloc(size);
        if (!read_data) {
          printf("failed to allocate memory for reading\n");
          break;
        }
        if ((error = VMReadBytes(pid, addr, read_data, &size))) {
          print_process_data(processname, addr, read_data, size);
        } else
          printf("failed to read data from process\n");
        free(read_data);
        break;

      // write data to memory
      case 'w':

        // read the address
        fscanf(stdin, "%llX", &addr);

        // read the data
        size = read_stream_data(stdin, &write_data);

        // and write it
        if ((error = VMWriteBytes(pid, addr, write_data, size)))
          printf("wrote %llX bytes\n", size);
        else
          printf("failed to write data to process\n");
        free(write_data);
        break;

      // write file to memory
      case 'W':

        // read the parameters
        read_addr_size(addr, size);
        filename = read_string_delimited(stdin, '\n', 0);
        trim_spaces(filename);

        // write the file
        write_file_to_process(filename, size, pid, addr);
        free(filename);

        break;

      // freeze value with immediate data
      case 'f':

        // if a quote is given, read the name
        while ((command = fgetc(stdin)) == ' ');
        char* freeze_name = NULL;
        if ((command == '\'') || (command == '\"')) {
          freeze_name = read_string_delimited(stdin, command, 1);
        } else
          ungetc(command, stdin);

        // read the address
        fscanf(stdin, "%llX", &addr);

        // read the data
        size = read_stream_data(stdin, &write_data);

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

        // read the address
        if (fgetc(stdin) != '\n') {
          fscanf(stdin, "%lld", &addr);
          if (UnfreezeRegionByIndex(addr))
            printf("failed to unfreeze region\n");
          else
            printf("region unfrozen\n");
        } else {
          ungetc('\n', stdin);
          printf("frozen regions:\n");
          PrintFrozenRegions(0);
        }
        break;

      // begin new search
      case 'S':

        // read the type
        arguments = read_string_delimited(stdin, '\n', 0);
        trim_spaces(arguments);

        // no arguments: show the list of searches
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
        const char* name = skip_word(arguments);

        // make a new search
        search = CreateNewSearchByTypeName(arguments, name);
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

        free(arguments);
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
        arguments = read_string_delimited(stdin, '\n', 0);
        trim_spaces(arguments);
        int pred = GetPredicateByName(arguments);

        // check if a value followed the predicate
        void* value = NULL;
        size = 0;
        char* value_text = skip_word(arguments);
        if (*value_text) {

          // we have a value... blargh
          unsigned long long ivalue;
          float fvalue;
          double dvalue;

          if (IsIntegerSearchType(search->type)) {
            sscanf(value_text, "%lld", &ivalue);
            value = &ivalue;
          } else if (search->type == SEARCHTYPE_FLOAT) {
            sscanf(value_text, "%f", &fvalue);
            value = &fvalue;
          } else if (search->type == SEARCHTYPE_DOUBLE) {
            sscanf(value_text, "%lf", &dvalue);
            value = &dvalue;
          } else if (search->type == SEARCHTYPE_DATA) {
            size = read_string_data(value_text, strlen(value_text),
                                    (unsigned char*)value_text);
            value = value_text;
          }
        }

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          free(arguments);
          break;
        }

        // run the search
        MemorySearchData* search_result = ApplyMapToSearch(search, map, pred,
                                                           value, size);
        map = NULL;
        free(arguments);

        // success? then save the result
        if (!search_result) {
          printf("search failed\n");
          break;
        }

        search = search_result;
        AddSearchToList(searches, search);
        if (search->numResults >= 20) {
          printf("results: %lld\n", search->numResults);
          break;
        }

        // fall through to display results

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
        arguments = read_string_delimited(stdin, '\n', 0);
        trim_spaces(arguments);

        // no name present? then we're deleting the current search
        if (!arguments[0]) {
          free(arguments);

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

      // pause process
      case '-':
        VMStopProcess(pid);
        process_running = 0;
        printf("process suspended\n");
        break;

      // resume process
      case '+':
        VMContinueProcess(pid);
        process_running = 1;
        printf("process resumed\n");
        break;

      // end-of-line, herp derp
      case '\n':
        ungetc('\n', stdin);
        break;

      // quit
      case 'q':
      case 'Q':
        command = 0; // tell the outer loop to quit
        break;

      // unknown
      default:
        printf("unknown command - try \'h\'\n");
    }

    // read extra junk at end of command
    while (fgetc(stdin) != '\n')
      usleep(10000);
  }

  // shut down the region freezer and return
  DeleteSearchList(searches);
  CleanupRegionFreezer();
  return 0;
}
