#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vmmap.h"
#include "freeze_region.h"
#include "parse_utils.h"

static pthread_t _freezeThread;
static pthread_mutex_t _mutex;
static int _freezerClosing = 0;
static int _numFrozenRegions = 0;
static FrozenRegion* _frozen = NULL;

static void* _freeze_thread_routine(void* data) {

  int x;
  while (!_freezerClosing) {
    pthread_mutex_lock(&_mutex);
    for (x = 0; x < _numFrozenRegions; x++)
      _frozen[x].error = VMWriteBytes(_frozen[x].pid, _frozen[x].addr,
                                      _frozen[x].data, _frozen[x].size);
    pthread_mutex_unlock(&_mutex);
    usleep(10000);
  }
  return NULL;
}

// initializes the region freezer
int InitRegionFreezer() {

  // we're not closing
  _freezerClosing = 0;

  // create the frozen-region mutex
  pthread_mutex_init(&_mutex, NULL);

  // create the writer thread
  if (pthread_create(&_freezeThread, NULL, _freeze_thread_routine, NULL))
    return 3;

  return 0;
}

// shuts down the region freezer
void CleanupRegionFreezer() {

  // stop the writer thread
  _freezerClosing = 1;
  pthread_join(_freezeThread, NULL);

  // delete all the frozen regions
  if (_frozen) {
    int x;
    for (x = 0; x < _numFrozenRegions; x++)
      free(_frozen[x].data);
    free(_frozen);
  }

  _frozen = NULL;
  _numFrozenRegions = 0;
}

// adds a region to the freeze-list
int FreezeRegion(pid_t pid, mach_vm_address_t addr, mach_vm_size_t size,
                 const void* data, const char* name) {

  // first unfreeze this address (in case it's already frozen)
  UnfreezeRegionByAddr(addr);

  // lock the freeze-list mutex so we can operate on it
  pthread_mutex_lock(&_mutex);

  // make room for this region in the list
  _frozen = (FrozenRegion*)realloc(_frozen, sizeof(FrozenRegion) *
                                   (_numFrozenRegions + 1));
  if (!_frozen) {
    pthread_mutex_unlock(&_mutex);
    return 1;
  }

  // fill in pid, addr and size
  _frozen[_numFrozenRegions].pid = pid;
  _frozen[_numFrozenRegions].addr = addr;
  _frozen[_numFrozenRegions].size = size;

  // make a copy of the data and append the name on the end
  _frozen[_numFrozenRegions].data = malloc(size + strlen(name) + 1);
  if (!_frozen[_numFrozenRegions].data) {
    pthread_mutex_unlock(&_mutex);
    return 2;
  }
  memcpy(_frozen[_numFrozenRegions].data, data, size);

  // set the name ptr and copy the name in
  _frozen[_numFrozenRegions].name =
    (char*)_frozen[_numFrozenRegions].data + size;
  strcpy(_frozen[_numFrozenRegions].name, name);
  _numFrozenRegions++;

  // unlock & return
  pthread_mutex_unlock(&_mutex);
  return 0;
}

// removes a region from the freeze-list. expects the freeze-list to be locked.
static int _UnfreezeRegionByIndexUnlocked(int index) {

  // index out of range? herp derp
  if (index < 0 || index >= _numFrozenRegions)
    return 1;

  // copy the regions back to remove the given index
  _numFrozenRegions--;
  memcpy(&_frozen[index], &_frozen[index + 1],
         sizeof(FrozenRegion) * (_numFrozenRegions - index));

  // realloc to save space
  _frozen = (FrozenRegion*)realloc(_frozen,
                                   _numFrozenRegions * sizeof(FrozenRegion));
  if (!_frozen)
    return 2;

  return 0;
}

// removes a region from the freeze-list
int UnfreezeRegionByIndex(int index) {

  // easy: lock, delete, and unlock
  pthread_mutex_lock(&_mutex);
  int rv = _UnfreezeRegionByIndexUnlocked(index);
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// removes a region by address from the freeze-list
int UnfreezeRegionByAddr(mach_vm_address_t addr) {

  // lock, find region by addr, delete, and unlock
  int x, rv = 0;
  pthread_mutex_lock(&_mutex);
  for (x = 0; x < _numFrozenRegions; x++)
    if (_frozen[x].addr == addr)
      break;
  if (x < _numFrozenRegions)
    rv = _UnfreezeRegionByIndexUnlocked(x);
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// removes a region from the freeze-list by name
int UnfreezeRegionByName(const char* name) {

  // lock, find region by name, delete, and unlock
  int x, rv = 0;
  pthread_mutex_lock(&_mutex);
  for (x = 0; x < _numFrozenRegions; x++)
    if (!strcmp(_frozen[x].name, name))
      break;
  if (x < _numFrozenRegions)
    rv = _UnfreezeRegionByIndexUnlocked(x);
  pthread_mutex_unlock(&_mutex);
  return rv;
}

// prints the list of frozen regions
void PrintFrozenRegions(int printData) {

  // lock, print, unlock
  int x;
  pthread_mutex_lock(&_mutex);
  for (x = 0; x < _numFrozenRegions; x++) {
    printf("%3d: %6u %016llX:%016llX [%d] %s\n", x, _frozen[x].pid,
           _frozen[x].addr, _frozen[x].size, _frozen[x].error, _frozen[x].name);
    if (printData)
      CRYPT_PrintData(_frozen[x].addr, _frozen[x].data, _frozen[x].size, 0);
  }
  pthread_mutex_unlock(&_mutex);  
}
