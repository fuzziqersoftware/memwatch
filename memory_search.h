// memory_search.h, by Martin Michelsen, 2012
// interface for interactive memory search

#ifndef MEMORY_SEARCH_H
#define MEMORY_SEARCH_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"

int memory_search(pid_t pid, int pause_during);

#endif // MEMORY_SEARCH_H
