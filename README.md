# memwatch
memwatch provides an interactive interface to find and modify variables in the memory of a running process. This is done by repeatedly searching the process' address space for values that follow the rules given by the user, narrowing down the result set until it includes only the desired variable. After finding this variable, memwatch can monitor it, change its value, "freeze" its value, and more. This functionality is similar to that provided by scanmem(1).

I often use memwatch to cheat in games, but have also used it in debugging to complement the features of a generic debugger such as gdb.

## Building

- Build and install phosg (https://github.com/fuzziqersoftware/phosg)
- `make`
- `sudo make install`

If it doesn't work on your system, let me know. It should build and run on all recent OS X versions.

## Running

Run `sudo memwatch <target_name_or_pid>`. If you give a name, memwatch will search the list of running processes to find one that matches this name (case-insensitive). If multiple processes match the name, it will fail - try giving a pid instead. See the man page (`man memwatch` after building and installing) for more options.

Upon running memwatch, you'll see a debugger-like prompt like `memwatch:48536/VirtualBoxVM 0s/0f # `. This prompt shows the pid of the attached process, the process name, the number of open searches, and the number of frozen memory regions.

## Commands

### General commands
- `help`: Displays the memwatch manual page.
- `attach [pid_or_name]`: Attaches to a new process by PID or by name. If no argument is given, attaches to a process with the same name as the currently-attached process.
- `data <data_string>`: Parses the data string and displays the raw values returned from the parser. You can use this to test complex data strings before writing them to be sure the correct data will be written.
- `state [field_name value]`:  With no arguments, displays simple variables from memwatch's internal state. This includes variables from command-line options, like use_color and pause_target. With arguments, sets the internal variable `field_name` to `value`.
- `quit`: Exits memwatch.

### Memory access commands
memwatch's interactive interface implements the following commands for reading, writing, and otherwise manipulating virtual memory:
- `access <address> <mode>`: Changes the virtual memory protection mode on the region containing `address`. The access mode is a string consisting of the r, w, or x characters, or some combination thereof. To remove all access to a memory region, use - for the access mode.
- `list`: Lists memory regions allocated in the target process's address space.
- `dump [filename_prefix]`: If a prefix is given, dumps all readable memory in the target process to files named `<prefix>.address1.address2.bin`, along with an index file. If no prefix is given, only determines which regions are readable.
- `read <address> <size> [filename]`: Reads a block of `size` bytes from `address` in the target process's memory. `address` and `size` are specified in hexadecimal. If a filename is given, writes the data to the given file instead of printing to the terminal. The `read` command may be prepended with `watch` to repeat the read every second until Ctrl+C is pressed.
- `write <address-or-result-id> <data>`: Writes `data` to `address` in the target process's memory. `address` may be preceded by `s` to read the address from the current search result set, or by `t` to read the address from the results of the previous invocation of the `find` command. See the Data Format section for information on how to specify the data string.

### Memory search commands
memwatch's interactive interface implements the following commands for searching a process's memory for variables:
- `find <data>`: Finds all occurrences of `data` in readable regions of the target's memory. See the Data Format section for more information on how to specify the search string. Searches done with this command do not affect the current saved search results.
- `open <type> [name]`: Opens a search for a variable of the given type in writable memory regions. `type` may be suffixed with a ! to search all readable memory instead of only writable memory. See the Search Types section for valid search types. If `name` is not given, the search will be unnamed and cannot be resumed after another search is opened.
- `open [name]`: If `name` is given, reopens a previous search. If `name` is not given, lists all open searches.
- `close [name]`: If `name` is given, closes the specified search. If `name` is not given, closes the current search.
- `fork <name> [name2]`: If `name2` is given, makes a copy of the search named `name` as `name2`. Otherwise, makes a copy of the current search as `name`.
- `search [search_name] <operator> <value>`: Reads the values of variables in the current list of results (or the named search's results, if a name is given), and filters out the results for which `new_value operator prev_value` is false. If `value` is not given, uses the value of the variable during the previous search. Valid operators are `<` (less than), `>` (greater than), `<=` (less-or-equal), `>=` (greater-or-equal), `=` (equal), `!=` (not equal), and `$` (flag search - true if the two arguments differ in only one bit). The `$` operator cannot be used in a search for a floating-point variable.
- `results [search_name]`: Displays the current list of results. If `search_name` is given, displays the results for that search. The command may be prepended with `watch` to read new values every second.
- `delete <addr1> [addr2]`: If `addr2` is given, deletes all results between `addr1` and `addr2`. If `addr2` is not given, deletes the search result at `addr1`.
- `set <value>`: Writes `value` to all addresses in the current result set.

### Memory freeze commands

memwatch implements a memory freezer, which repeatedly writes values to the target's memory at a very short interval, effectively fixing the variable's value in the target process. The following commands allow manipulation of frozen variables:
- `freeze [n<name>] @<address-or-result-id> x<data>`: Sets a freeze on `address` with the given data. `address` may be preceded by `s` to read the address from the current search result set, or by `t` to read the address from the results of the previous invocation of the `find` command. The given data is written in the background approximately every 10 milliseconds. Sets the freeze name to `name` if given; otherwise, sets the freeze name to the current search name (if any). `data` may not contain spaces.
- `freeze [n<name>] @<address-or-result-id> s<size>`: Identical to the above command, but uses the data already present in the process's memory. Size is specified in hexadecimal.
- `freeze [n<name>] @<address-or-result-id> m<max-entries> x<data> [N<null-data>]`: Sets a freeze on an array of `max-entries` items starting at `address` with the given data. If `data` is not present in the array, the first null entry in the array is overwritten with `data`. Null entries are those whose contents are entirely zeroes, or whose contents match `null-data` if `null-data` is given. The size of each array element is assumed to be the size of `data`. `data` and `null-data` must have equal sizes.
- `unfreeze [id]`: If `id` is not given, displays the list of currently-frozen regions. Otherwise, `id` may be the index, address, or name of the region to unfreeze. If a name is given and multiple regions have the same name, unfreezes all of them. If `*` is given, unfreezes all regions.
- `frozen [data | commands]`: Displays the list of currently-frozen regions. If run as `frozen data`, displays the data associated with each region as well. If run as `frozen commands`, displays for each frozen region a command to freeze that region (this is generally a more concise way to view frozen regions with their data).

### Execution state management commands
memwatch implements experimental support for viewing and modifying execution state in the target process, implemented by the following commands:
- `pause`: Pauses the target process.
- `resume`: Unpauses the target process.
- `signal <signum>`: Sends the Unix signal `signum` to the target process. See the signal(3) manual page for a list of signals.
- `regs`: Reads the register state for all threads in the target process. If the process is not paused, thread registers might not represent an actual overall state of the process at any point in time.
- `wregs <thread_id> <value> <reg>`: Writes `value` to `reg` in one thread of the target process. `thread_id` should match one of the thread IDs shown by the regs command.
- `stacks [size]`: Reads `size` bytes from the stack of each thread. If not given, `size` defaults to 0x100 (256 bytes).

## Search types
memwatch supports searching for the following types of variables. Any type except 'str' may be prefixed by the letter 'r' to perform reverse-endian searches (that is, to search for big-endian values on a little-endian architecture, or vice versa).
- `s`, `str`, `string`: Search for any string. Values are specified in immediate data format (see the Data Format section for more information).
- `f`, `flt`, `float`: Search for a 32-bit floating-point value.
- `d`, `dbl`, `double`: Search for a 64-bit floating-point value.
- `u8`, `u16`, `u32`, `u64`: Search for an unsigned 8-bit, 16-bit, 32-bit, or 64-bit value.
- `s8`, `s16`, `s32`, `s64`: Search for a signed 8-bit, 16-bit, 32-bit, or 64-bit value.

## Data format
Input data for raw data searches and the `find`, `write`, and `freeze` commands is specified in a custom format, described here. You can try using this format with the `data` command (see above). Every pair of hexadecimal digits represents one byte, with special control sequences as follows:

- **Decimal integers**: A decimal integer may be specified by preceding it with `#` signs (`#` for a single byte, `##` for a 16-bit int, `###` for a 32-bit int, or `####` for a 64-bit int).
- **Floating-point numbers**: A floating-point number may be specified by preceding it with `%` signs (`%` for single-precision, `%%` for double-precision).
- **String literals**: ASCII strings must be enclosed in double quotes, and unicode strings in single quotes. Within a string, the escape sequences `\n`, `\r`, `\t`, and `\\` will be replaced with a newline, a carriage return, a tab character, and a single backslash respectively.
- **File contents**: A string enclosed in `< >` will be treated as a filename, and will be replaced with the contents of the file in the output data.
- **Change of endianness**: A dollar sign (`$`) inverts the endianness of the data following it. This applies to unicode string literals, integers specified with `#` signs, and floating-point numbers.
- **Wildcard**: Any data between question marks (`?`) will match any byte when searching with the `find` command or freezing array entries with the `freeze` command. This is not yet implemented for the `search` command.
- **Comments**: Comments are formatted in C-style blocks; anything between `/*` and `*/` will be omitted from the output string, as well as anything between `//` and a newline (though this format is rarely used since commands are delimited by newlines). Comments cannot be nested.

Any non-recognized characters are ignored. The initial endian-ness of the output depends on the endian-ness of the host machine: on an Intel machine, the resulting data would be little-endian.

**Example data string:**
`/* omit 01 02 */ 03 ?04? $ ##30 $ ##127 ?"dark"? ###-1 'cold'`

**Resulting data (Intel):**
`03 04 00 1E 7F 00 64 61 72 6B FF FF FF FF 63 00 6F 00 6C 00 64 00`
**Resulting mask:**
`FF 00 FF FF FF FF 00 00 00 00 FF FF FF FF FF FF FF FF FF FF FF FF`

## Usage example
You're playing Supaplex in DOSBox and you want to have infinite bombs. It's not obvious what the data size is for this variable, but the value is always small, so a u8 search should find it. 
```
fuzziqersoftware@pointy:~$ sudo memwatch dosbox
```

Open a search for this variable:
```
memwatch:90732/DOSBox 0s/0f # open u8 bombs
opened new u8 search named bombs
```

Now start playing a level on which there are a lot of bombs. Collect a few of them (three is probably enough) and search for that:
```
memwatch:90732/DOSBox 1s/0f bombs # search = 3
results: 378052
```

Now use one of the bombs and narrow down the result set to variables that were 3 during the initial search and 2 now:
```
memwatch:90732/DOSBox 1s/0f bombs(378052) # s = 2
results: 167
```

Use another bomb and search again:
```
memwatch:90732/DOSBox 1s/0f bombs(167) # s = 1
(0) 000000000C34E37C 1 (0x01)
```

That must be the variable that represents the number of bombs you have. Now you can freeze that address at a nonzero value (`s0` refers to the first result in the current search):
```
memwatch:90732/DOSBox 1s/0f bombs(1) # freeze s0 01
region frozen
```

Now you have infinite bombs as long as memwatch is running (and you don't unfreeze that variable).