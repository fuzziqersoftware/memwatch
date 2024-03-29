# memwatch
memwatch provides an interactive interface to find and modify variables in the memory of a running process. This is done by repeatedly searching the process' address space for values that follow the rules given by the user, narrowing down the result set until it includes only the desired variable. After finding this variable, memwatch can monitor it, change its value, "freeze" its value, and more. This functionality is similar to that provided by scanmem(1).

I often use memwatch to cheat in games, but have also used it in debugging to complement the features of a generic debugger such as gdb.

## Building

- Build and install phosg (https://github.com/fuzziqersoftware/phosg)
- Build and install libamd64 (https://github.com/fuzziqersoftware/libamd64)
- `make`
- `sudo make install`

If it doesn't work on your system, let me know. It should build and run on all recent OS X versions.

## Running

Run `sudo memwatch <target_name_or_pid>`. If you give a name, memwatch will search the list of running processes to find one that matches this name (case-insensitive). If multiple processes match the name, it will ask you which one to operate on. See the man page (`man memwatch` after building and installing) for more options.

Upon running memwatch, you'll see a debugger-like prompt like `memwatch:48536/VirtualBoxVM 0s/0f # `. This prompt shows the pid of the attached process, the process name, the number of open searches, and the number of frozen memory regions.

## Commands

Most commands can be abbreviated intuitively (e.g. `read`, `rd`, and `r` are all the same command). There are a few unintuitive abbreviations, which are noted below.

### General commands

- `attach [pid_or_name]`: Attaches to a new process by PID or by name. If no argument is given, attaches to a process with the same name as the currently-attached process.
- `data <data_string>`: Parses the data string and displays the raw values returned from the parser. You can use this to test complex data strings before writing them to be sure the correct data will be written.
- `state [field_name value]`:  With no arguments, displays simple variables from memwatch's internal state. This includes variables from command-line options, like use_color and pause_target. With arguments, sets the internal variable `field_name` to `value`.
- `watch <command> [args...]` or `! <command> [args...]`: Repeat the given command every second until Ctrl+C is pressed. Not all commands may be repeated; if the command doesn't support watching, it will execute only once.
- `quit` or `exit`: Exits memwatch.

### Memory access commands

memwatch's interactive interface implements the following commands for reading, writing, and otherwise manipulating virtual memory:
- `access <address> <mode>`: Changes the virtual memory protection mode on the region containing `address`. The access mode is a string consisting of the r, w, or x characters, or some combination thereof. To remove all access to a memory region, use - for the access mode.
- `list`: Lists memory regions allocated in the target process's address space.
- `dump [filename_prefix]`: If a prefix is given, dumps all readable memory in the target process to files named `<prefix>.address1.address2.bin`, along with an index file. If no prefix is given, only determines which regions are readable.
- `read <address> <size> [filename] [+format]`: Reads a block of `size` bytes from `address` in the target process's memory. `address` and `size` are specified in hexadecimal. If a filename is given, writes the data to the given file instead of printing to the terminal. `format` is a short string specifying which additional interpretations of the data should be printed. It starts with '+' and includes any of the characters 'a' (text), 'f' (float), 'd' (double), and 'r' (reverse-endian); the default is '+a'. 
- `write <address-or-result-id> <data>`: Writes `data` to `address` in the target process's memory. `address` may be preceded by `s` to read the address from the current search result set, or by `t` to read the address from the results of the previous invocation of the `find` command. For example, specifying `s0` refers to the first result address in the current search; `s1` refers to the second result, etc. Results from other searches may also be used by specifying the search name explicitly, as in `s1@search-name`.  See the Data Format section for information on how to specify the data string.

### Memory search commands

memwatch's interactive interface implements the following commands for searching a process's memory for variables:
- `find <data>` or `t <data>`: Finds all occurrences of `data` in readable regions of the target's memory. See the Data Format section for more information on how to specify the search string. Searches done with this command do not affect the current saved search results.
- `open <type> [name]`: Opens a search for a variable of the given type in writable memory regions. `type` may be suffixed with a ! to search all readable memory instead of only writable memory. See the Search Types section for valid search types. If `name` is not given, the search will be unnamed and cannot be resumed after another search is opened. If `predicate` is given, perform the initial search immediately.
- `open [name]`: If `name` is given, reopens a previous search. If `name` is not given, lists all open searches.
- `close [name]`: If `name` is given, closes the specified search. If `name` is not given, closes the current search.
- `fork <name> [name2]`: If `name2` is given, makes a copy of the search named `name` as `name2`. Otherwise, makes a copy of the current search as `name`.
- `search [search_name] <operator> [value]`: Reads the values of variables in the current list of results (or the named search's results, if a name is given), and filters out the results for which `new_value operator prev_value` is false. If `value` is not given, uses the value of the variable during the previous search. Valid operators are `<` (less than), `>` (greater than), `<=` (less-or-equal), `>=` (greater-or-equal), `=` (equal), `!=` (not equal), and `$` (flag search - true if the two arguments differ in only one bit). The `$` operator cannot be used in a search for a floating-point variable.
- `search [search_name] .`: Begins a search for a variable with an unknown initial value. Once this is done, future searches can be done using the above operators.
- `results [search_name]` or `x`: Displays the current list of results. If `search_name` is given, displays the results for that search.
- `delete <spec> [spec ...]`: Deletes specific search results. `spec` may be the address of a specific result to delete, or a range of addresses to delete, which is inclusive on both ends. Ranges are specified as a pair of addresses separated by a dash with no spaces. Result references like `s1` are acceptable for this command as well.
- `iterations [search_name]`: Displays the list of stored iterations for the current search, or the named search if a name is given.
- `truncate [search_name] <count>`: Deletes all iterations except the `count` most recent from the current search, or the named search if a name is given.
- `undo [search_name]` or `cz`: Undoes the latest iteration of the current search, or the named search if a name is given.
- `set <value>`: Writes `value` to all addresses in the current result set.
- `set <result-id> <value>`: Writes `value` to one address in the current result set. `result-id` should be of the form `s0`, `s1`, etc. (as for the `write` command).

### Memory freeze commands

memwatch implements a memory freezer, which repeatedly writes values to the target's memory at a very short interval, effectively fixing the variable's value in the target process. The following commands allow manipulation of frozen variables:
- `freeze [+n<name>] <address-or-result-id> <data> [+d]`: Sets a freeze on `address` with the given data. `address` may refer to a search or find result, using the same syntax as for the `write` command. The given data is written in the background approximately every 10 milliseconds. Sets the freeze name to `name` if given; otherwise, sets the freeze name to the current search name (if any). If `+d` is given, the freeze is initially disabled and won't take effect until enabled with the `enable` command.
- `freeze [+n<name>] <address-or-result-id> +s<size> [+d]`: Identical to the above command, but uses the data already present in the process's memory. Size is specified in hexadecimal.
- `freeze [+n<name>] <address-or-result-id> +m<max-entries> <data> [+N<null-data>] [+d]`: Sets a freeze on an array of `max-entries` items starting at `address` with the given data. If `data` is not present in the array, the first null entry in the array is overwritten with `data`. Null entries are those whose contents are entirely zeroes, or whose contents match `null-data` if `null-data` is given. The size of each array element is assumed to be the size of `data`. `data` and `null-data` must have equal sizes.
- `unfreeze [id]`: If `id` is not given, displays the list of currently-frozen regions. Otherwise, `id` may be the index, address, or name of the region to unfreeze. If a name is given and multiple regions have the same name, unfreezes all of them. If `*` is given, unfreezes all regions.
- `enable <id>` or `+ <id>`: Enables the given frozen regions, so their values will be written. Values for `id` are specified in the same way as for the `unfreeze` command.
- `disable <id>` or `- <id>`: Disables the given frozen regions, so their values will not be written. Values for `id` are specified in the same way as for the `unfreeze` command.
- `frozen [data | commands]`: Displays the list of currently-frozen regions. If run as `frozen data`, displays the data associated with each region as well. If run as `frozen commands`, displays for each frozen region a command to freeze that region (this is generally a more concise way to view frozen regions with their data).

### Execution state management commands

memwatch implements experimental support for viewing and modifying execution state in the target process, implemented by the following commands:
- `pause`: Pauses the target process.
- `unpause` or `resume`: Unpauses the target process.
- `signal <signum>`: Sends the Unix signal `signum` to the target process. See the signal(3) manual page for a list of signals.

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

## Usage examples

### Known-value search
We're playing Supaplex in DOSBox and we want to have infinite bombs. The number of bombs that we have is displayed on the screen at all times, so we can always know how many the game thinks we have. We start memwatch and open an unsigned, small integer search:

    fuzziqersoftware@pointy:~$ sudo memwatch dosbox
    memwatch:90732/DOSBox 0s/0f # open u8 bombs
    opened new u8 search named bombs

Now we start playing a level on which there are a lot of bombs. We collect a few of them and search for the number we collected:

    memwatch:90732/DOSBox 1s/0f bombs:u8 # search = 3
    memwatch:90732/DOSBox 1s/0f bombs:u8(378052) #

Now we use one of the bombs, and narrow down the result set to variables that had the value 3 during the initial search but have the value 2 now:

    memwatch:90732/DOSBox 1s/0f bombs:u8(378052) # s = 2
    memwatch:90732/DOSBox 1s/0f bombs:u8(167) #

We use one more bomb and search again:

    memwatch:90732/DOSBox 1s/0f bombs:u8(167) # s = 1
    (0) 000000000C34E37C 1 (0x01)
    memwatch:90732/DOSBox 1s/0f bombs:u8(1) #

This single result must be the variable that represents the number of bombs we have. Now we freeze that address at a nonzero value (`s0` refers to the first result in the current search):

    memwatch:90732/DOSBox 1s/0f bombs:u8(1) # freeze s0 01
    region frozen

Now we have infinite bombs as long as memwatch is running, and we don't unfreeze that variable.

### Unknown-value search

Paper Mario for the Nintendo 64 has a glitch whereby we can replay earlier chapters of the game, but keep all of our items and upgrades from later chapters. To trigger the glitch, we need to fall under the floor in a specific room. This is possible to do without memory editing, but very difficult to pull off without spending a lot of time setting up the necessary glitches. Instead, we can use memwatch to directly change our Y coordinate within the emulator.

In this example we're using sixtyforce, but this should work for any emulator. The Nintendo 64 uses 32-bit registers, so we assume the type we need is `float`. We start memwatch, open a search, and do an initial-value search:

    fuzziqersoftware@pointy:~$ sudo memwatch sixtyforce
    memwatch:91372/sixtyforce 0s/0f # open float y-coord
    memwatch:91372/sixtyforce 1s/0f y-coord:f # s .
    memwatch:91372/sixtyforce 1s/0f y-coord:f(+) #

This creates a reference point for the next search, but doesn't generate any results yet - `(+)` indicates that a reference point exists for the current search. Now we jump up onto a ledge (which should increase our Y coordinate within the game) and search for increased values:

    memwatch:91372/sixtyforce 1s/0f y-coord:f(+) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(192462) #

Unknown-value searches tend to take longer to converge to the variables we want. We jump onto ledges, jump down, jump up again, etc., repeating until we get a manageable number of search results:

    memwatch:91372/sixtyforce 1s/0f y-coord:f(192462) # s <
    memwatch:91372/sixtyforce 1s/0f y-coord:f(55813) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(24824) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(5594) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(1336) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(327) # s >
    memwatch:91372/sixtyforce 1s/0f y-coord:f(82) # s <
    memwatch:91372/sixtyforce 1s/0f y-coord:f(72) # s =
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # s >
    (0) 00000001070177A0 50.000000 (0x42480000)
    (1) 00000001082B60C0 50.000000 (0x42480000)
    (2) 00000001082F3DC0 166.129028 (0x43262108)
    (3) 00000001082F3DE4 50.000000 (0x42480000)
    (4) 00000001082F4214 50.000000 (0x42480000)
    (5) 00000001082F4228 50.000000 (0x42480000)
    (6) 0000000108350FF4 50.000000 (0x42480000)
    (7) 000000010839C580 50.000000 (0x42480000)
    (8) 00000001085534E4 50.000000 (0x42480000)
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) #

Now we can try changing each of these individually to see if they have any effect in the game:

    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s0 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s1 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s2 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s3 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s4 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s5 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s6 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s7 100
    wrote 4 (0x4) bytes
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s8 100
    wrote 4 (0x4) bytes

When we wrote to `s6`, we suddenly jumped up into the air within the game - this must be our Y coordinate. Now that we know this, we go to the place where we want to fall under the floor, check the value at that place, and set it to a smaller value:

    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # x
    (0) 00000001070177A0 0.000000 (0x00000000)
    (1) 00000001082B60C0 0.000000 (0x00000000)
    (2) 00000001082F3DC0 116.129028 (0x42E84210)
    (3) 00000001082F3DE4 0.000000 (0x00000000)
    (4) 00000001082F4214 0.000000 (0x00000000)
    (5) 00000001082F4228 0.000000 (0x00000000)
    (6) 0000000108350FF4 0.000000 (0x00000000)
    (7) 000000010839C580 0.000000 (0x00000000)
    (8) 00000001085534E4 0.000000 (0x00000000)
    memwatch:91372/sixtyforce 1s/0f y-coord:f(9) # set s6 -20
    wrote 4 (0x4) bytes

This causes us to fall under the floor and trigger the beginning of the story again.
