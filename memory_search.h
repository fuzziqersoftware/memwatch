// memory_search.h, by Martin Michelsen, 2012
// interface for interactive memory search

#ifndef MEMORY_SEARCH_H
#define MEMORY_SEARCH_H

#include <sys/types.h>
#include <stdint.h>

int prompt_for_commands(pid_t pid, int freeze_while_operating, uint64_t max_results);

#endif // MEMORY_SEARCH_H
