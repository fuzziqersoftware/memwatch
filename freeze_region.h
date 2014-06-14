// freeze_region.h, by Martin Michelsen, 2012
// library to freeze memory locations in other processes

#ifndef FREEZE_REGION_H
#define FREEZE_REGION_H

#include <mach/mach_vm.h>
#include <sys/types.h>

typedef struct {
  pid_t pid;
  mach_vm_address_t addr;
  mach_vm_size_t size;
  int error;
  int max_array_size;
  void* data;
  void* array_null_data;
  void* array_temp_data;
  char* name;
} FrozenRegion;

int freeze_init();
void freeze_exit();

int freeze_region(pid_t pid, mach_vm_address_t addr, mach_vm_size_t size,
    const void* data, int max_array_size, const void* array_null_data,
    const char* name);
int unfreeze_by_index(int index);
int unfreeze_by_addr(mach_vm_address_t addr);
int unfreeze_by_name(const char* name);
void move_frozen_regions_to_process(pid_t pid);

void print_frozen_regions(int _print_data);
int get_num_frozen_regions();

#endif // FREEZE_REGION_H
