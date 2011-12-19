#ifndef MEMORY_SEARCH_H
#define MEMORY_UTILS_H

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"

int memory_search(pid_t pid, int pause_during);

#endif // MEMORY_UTILS_H
