#ifndef FREEZE_THREAD_H
#define FREEZE_THREAD_H

#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"

typedef struct {
  pid_t pid;
  mach_vm_address_t addr;
  mach_vm_size_t size;
  int error;
  void* data;
  char* name;
} FrozenRegion;

int InitRegionFreezer();
int CleanupRegionFreezer();

int FreezeRegion(pid_t pid, mach_vm_address_t addr, mach_vm_size_t size,
                 const void* data, const char* name);
int UnfreezeRegionByIndex(int index);
int UnfreezeRegionByAddr(mach_vm_address_t addr);
int UnfreezeRegionByName(const char* name);

void GetFrozenRegions(const FrozenRegion** regions, int* numRegions);

#endif // FREEZE_THREAD_H
