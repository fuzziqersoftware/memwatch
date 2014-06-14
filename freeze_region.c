// freeze_region.c, by Martin Michelsen, 2012
// library to freeze memory locations in other processes

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "freeze_region.h"
#include "parse_utils.h"
#include "vmmap.h"

// thread object for the freezer thread
static pthread_t _freeze_thread;

// lock that protects the list of frozen regions
static pthread_mutex_t _mutex;

// set to 1 when freezer is shutting down
static int _freezer_closing = 0;

// list of frozen regions
static int _num_frozen_regions = 0;
static FrozenRegion* _frozen = NULL;

static int _freeze_update_region(FrozenRegion* rgn) {
  if (rgn->max_array_size) {

    // read the entire array contents
    mach_vm_size_t array_size = rgn->max_array_size * rgn->size;
    if (!rgn->array_temp_data)
      rgn->array_temp_data = malloc(array_size);
    if (!rgn->array_temp_data)
      return -1;
    VMReadBytes(rgn->pid, rgn->addr, rgn->array_temp_data, &array_size);
    if (array_size != rgn->max_array_size * rgn->size) {
      return -2;
    }

    // find the first null entry in the array, OR find an entry that matches
    // the frozen data
    int x, y;
    for (x = 0; x < rgn->max_array_size; x++) {
      void* array_pos_local = (char*)rgn->array_temp_data + (x * rgn->size);

      if (!memcmp(array_pos_local, rgn->data, rgn->size))
        return 0; // data already present in array

      if (rgn->array_null_data) {
        if (!memcmp(array_pos_local, rgn->array_null_data, rgn->size))
          break;
      } else {
        for (y = 0; y < rgn->size; y++)
          if (((char*)array_pos_local)[y] != 0)
            break;
        if (y == rgn->size)
          break;
      }
    }

    // no null entries? then don't write anything
    if (x == rgn->max_array_size)
      return -3;

    // else, write to the first null entry
    return VMWriteBytes(rgn->pid, rgn->addr + (x * rgn->size), rgn->data,
        rgn->size);

  } else {
    return VMWriteBytes(rgn->pid, rgn->addr, rgn->data, rgn->size);
  }
}

// freezer routine. repeatedly writes data to processes based on the frozen
// region list, until told to stop via _freezer_closing.
static void* _freeze_thread_routine(void* data) {

  int x;
  while (!_freezer_closing) {
    pthread_mutex_lock(&_mutex);
    for (x = 0; x < _num_frozen_regions; x++)
      _frozen[x].error = _freeze_update_region(&_frozen[x]);
    pthread_mutex_unlock(&_mutex);
    usleep(10000);
  }
  return NULL;
}

// initializes the region freezer
int freeze_init() {

  // we're not closing
  _freezer_closing = 0;

  // create the frozen-region mutex
  pthread_mutex_init(&_mutex, NULL);

  // create the freezer thread
  if (pthread_create(&_freeze_thread, NULL, _freeze_thread_routine, NULL))
    return 3;

  return 0;
}

// shuts down the region freezer
void freeze_exit() {

  // stop the writer thread
  _freezer_closing = 1;
  pthread_join(_freeze_thread, NULL);

  // delete all the frozen regions
  if (_frozen) {
    int x;
    for (x = 0; x < _num_frozen_regions; x++)
      free(_frozen[x].data);
    free(_frozen);
  }

  _frozen = NULL;
  _num_frozen_regions = 0;
}

// adds a region to the freeze-list
int freeze_region(pid_t pid, mach_vm_address_t addr, mach_vm_size_t size,
    const void* data, int max_array_size, const void* array_null_data,
    const char* name) {

  // first unfreeze this address (in case it's already frozen)
  unfreeze_by_addr(addr);

  // lock the freeze-list mutex so we can operate on it
  pthread_mutex_lock(&_mutex);

  // make room for this region in the list
  _frozen = (FrozenRegion*)realloc(_frozen, sizeof(FrozenRegion) *
                                   (_num_frozen_regions + 1));
  if (!_frozen) {
    pthread_mutex_unlock(&_mutex);
    return 1;
  }
  FrozenRegion* this_region = &_frozen[_num_frozen_regions];

  // fill in pid, addr and sizes
  this_region->pid = pid;
  this_region->addr = addr;
  this_region->size = size;
  this_region->max_array_size = max_array_size;
  this_region->array_temp_data = NULL;

  // make a copy of the data and append the name on the end
  int data_size = size + strlen(name) + 1;
  if (array_null_data)
    data_size += size;
  this_region->data = malloc(data_size);
  if (!this_region->data) {
    pthread_mutex_unlock(&_mutex);
    return 2;
  }
  memcpy(this_region->data, data, size);

  // if array_null_data is given, copy it in
  if (array_null_data) {
    this_region->array_null_data = (char*)this_region->data + size;
    memcpy(this_region->array_null_data, array_null_data, size);
    this_region->name = (char*)this_region->array_null_data + size;
  } else {
    this_region->array_null_data = NULL;
    this_region->name = (char*)this_region->data + size;
  }

  // copy the name in
  strcpy(this_region->name, name);
  _num_frozen_regions++;

  // unlock & return
  pthread_mutex_unlock(&_mutex);
  return 0;
}

// removes a region from the freeze-list. expects the freeze-list to be locked.
static int _unfreeze_by_index_unlocked(int index) {

  // index out of range? herp derp
  if (index < 0 || index >= _num_frozen_regions)
    return 1;

  // free the allocated data
  if (_frozen[index].data)
    free(_frozen[index].data);
  if (_frozen[index].array_temp_data)
    free(_frozen[index].array_temp_data);

  // copy the regions back to remove the given index
  _num_frozen_regions--;
  memcpy(&_frozen[index], &_frozen[index + 1],
         sizeof(FrozenRegion) * (_num_frozen_regions - index));

  // realloc to save space
  _frozen = (FrozenRegion*)realloc(_frozen,
                                   _num_frozen_regions * sizeof(FrozenRegion));
  if (!_frozen)
    return 2;

  return 0;
}

// removes a region from the freeze-list
// returns an error code, or 0 on success
int unfreeze_by_index(int index) {

  // easy: lock, delete, and unlock
  pthread_mutex_lock(&_mutex);
  int rv = _unfreeze_by_index_unlocked(index);
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// removes a region by address from the freeze-list
// returns an error code, or 0 on success
int unfreeze_by_addr(mach_vm_address_t addr) {

  // lock the region list
  int x, rv = 0;
  pthread_mutex_lock(&_mutex);

  // find region by address
  for (x = 0; x < _num_frozen_regions; x++)
    if (_frozen[x].addr == addr)
      break;

  // found it? then delete it from the list
  // didn't find it? return 1
  if (x < _num_frozen_regions)
    rv = _unfreeze_by_index_unlocked(x);
  else
    rv = 1; // no such region

  // unlock the region list & return
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// removes a region from the freeze-list by name
// returns the number of regions unfrozen
int unfreeze_by_name(const char* name) {

  // lock the region list
  int x, rv = 0;
  pthread_mutex_lock(&_mutex);

  // find and unlock regions by name. unlike other unlock functions, return the
  // number of regions unfrozen
  for (x = 0; x < _num_frozen_regions; x++) {
    if (!strcmp(_frozen[x].name, name)) {
      if (!_unfreeze_by_index_unlocked(x)) {
        x--;
        rv++;
      }
    }
  }

  // unlock the region list and return
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// moves all frozen regions to a new process id
void move_frozen_regions_to_process(pid_t pid) {
  int x;
  for (x = 0; x < _num_frozen_regions; x++)
    _frozen[x].pid = pid;
}

// prints the list of frozen regions
void print_frozen_regions(int _print_data) {

  // lock the region list
  int x;
  pthread_mutex_lock(&_mutex);

  // if there are any frozen regions, print a list of them
  if (_num_frozen_regions > 0) {
    printf("frozen regions:\n");
    for (x = 0; x < _num_frozen_regions; x++) {
      if (_frozen[x].max_array_size)
        printf("%3d: %6u %016llX:%016llX [array:%d] [%d] %s\n", x,
            _frozen[x].pid, _frozen[x].addr, _frozen[x].size,
            _frozen[x].max_array_size, _frozen[x].error, _frozen[x].name);
      else
        printf("%3d: %6u %016llX:%016llX [%d] %s\n", x, _frozen[x].pid,
            _frozen[x].addr, _frozen[x].size, _frozen[x].error,
            _frozen[x].name);
      if (_print_data) {
        print_data(_frozen[x].addr, _frozen[x].data, NULL, _frozen[x].size, 0);
        if (_frozen[x].array_null_data)
          print_data(_frozen[x].addr, _frozen[x].array_null_data, NULL,
              _frozen[x].size, 0);
      }
    }
  // if there are no frozen regions, too bad :(
  } else
    printf("no regions frozen\n");

  // unlock the region list
  pthread_mutex_unlock(&_mutex);  
}

// returns the number of frozen regions
int get_num_frozen_regions() {
  return _num_frozen_regions;
}