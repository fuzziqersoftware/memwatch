// memory_search.h, by Martin Michelsen, 2012
// interface for interactive memory search

#ifndef MEMORY_SEARCH_H
#define MEMORY_SEARCH_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"

int prompt_for_commands(pid_t pid);

#endif // MEMORY_SEARCH_H
