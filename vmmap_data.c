#include <stdlib.h>
#include <stdio.h>
#include "vmmap_data.h"

extern int* cancel_var;

VMRegionDataMap* GetProcessRegionList(pid_t pid, unsigned int modeMask) {

  // read the first region
  VMRegion vmr = VMNextRegion(pid, VMNullRegion);
  if (VMEqualRegions(vmr, VMNullRegion))
    return NULL;

  // alloc a new map header
  VMRegionDataMap* map = (VMRegionDataMap*)malloc(sizeof(VMRegionDataMap));
  if (!map)
    return NULL;
  map->process = pid;
  map->numRegions = 0;

  // now, for each region...
  int cont = 1;
  cancel_var = &cont;
  for (; !VMEqualRegions(vmr, VMNullRegion) && cont;
       vmr = VMNextRegion(pid, vmr)) {

    // is this the NULL region? skip it
    if (!vmr._address)
      continue;

    // wrong mode? don't include it
    if ((vmr._attributes & modeMask) != modeMask)
      continue;

    // alloc space for this region's metadata in the map
    map = (VMRegionDataMap*)realloc(map, sizeof(VMRegionDataMap) +
                                    sizeof(VMRegionData) * (map->numRegions + 1));
    if (!map) {
      cancel_var = NULL;
      return NULL;
    }

    // fill in region info and advance
    map->regions[map->numRegions].region._process = pid;
    map->regions[map->numRegions].region._address = vmr._address;
    map->regions[map->numRegions].region._size = vmr._size;
    map->regions[map->numRegions].region._attributes = vmr._attributes;
    map->regions[map->numRegions].data = NULL;
    map->numRegions++;
  }
  cancel_var = NULL;

  // if we were canceled, return NULL
  if (!cont) {
    DestroyDataMap(map);
    return NULL;
  }

  return map;
}

VMRegionDataMap* DumpProcessMemory(pid_t pid, unsigned int modeMask) {

  int x, error;

  // first get the list of regions
  VMRegionDataMap* map = GetProcessRegionList(pid, modeMask);
  if (!map)
    return NULL;

  // now, for each region...
  int cont = 1;
  cancel_var = &cont;
  for (x = 0; cont && (x < map->numRegions); x++) {

    // alloc space for this region's data
    map->regions[x].data = malloc(map->regions[x].region._size);
    if (!map->regions[x].data) {
      DestroyDataMap(map);
      cancel_var = NULL;
      return NULL;
    }

    // make the region readable if we have to
    if (!(map->regions[x].region._attributes & VMREGION_READABLE)) {
      error = VMSetRegionProtection(pid, map->regions[x].region._address,
          map->regions[x].region._size, VMREGION_READABLE, VMREGION_READABLE);
    }

    // read the region's contents!
    mach_vm_size_t local_size = map->regions[x].region._size;
    error = VMReadBytes(map->regions[x].region._process,
                        map->regions[x].region._address,
                        map->regions[x].data, &local_size);

    // if reading failed, don't include the region's data
    if (!error || (map->regions[x].region._size != local_size)) {
      // TODO: why does this error occur?
      free(map->regions[x].data);
      map->regions[x].data = NULL;
    }

    // if we made the region readable, restore its previous protection
    if (!(map->regions[x].region._attributes & VMREGION_READABLE)) {
      error = VMSetRegionProtection(pid, map->regions[x].region._address,
          map->regions[x].region._size, map->regions[x].region._attributes,
          VMREGION_ALL);
    }
  }
  cancel_var = NULL;
  
  // if we were canceled, return NULL
  if (!cont) {
    DestroyDataMap(map);
    return NULL;
  }
  
  return map;
}

// re-reads data in a data map from the remote process
int UpdateDataMap(VMRegionDataMap* map) {

  unsigned long x;
  int error;

  // for each region...
  int cont = 1;
  cancel_var = &cont;
  for (x = 0; x < map->numRegions; x++) {

    // read the data in this section
    mach_vm_size_t newsize = map->regions[x].region._size;
    error = VMReadBytes(map->process, map->regions[x].region._address,
                        map->regions[x].data, &newsize);

    // error? do something about it
    // TODO: should delete offending section
  }
  cancel_var = NULL;
  
  // if we were canceled, return error
  if (!cont) {
    DestroyDataMap(map);
    return 1;
  }
  
  return 0;
}

/*VMRegionDataMap* CopyDataMap(VMRegionDataMap* src) {
  VMRegionDataMap* new = (VMRegionDataMap*)malloc(sizeof(VMRegionDataMap) +
                  sizeof(VMRegionData) * src->numRegions);
  if (!new)
    return NULL;

  memcpy(new, src, sizeof(VMRegionDataMap));
  memset(new->regions, 0, sizeof(VMRegionData*) * new->numRegions);
  unsigned long x;
  for (x = 0; x < new->numRegions; x++) {
    new->regions[x].data = (VMRegionData*)malloc(src->regions[x].region._size);
    if (!new->regions[x]) {
      DestroyDataMap(new);
      return NULL;
    }
    memcpy(&new->regions[x], &src->regions[x], sizeof(VMRegionData) +
             src->regions[x].region._size);
  }

  return new;
} */

void DestroyDataMap(VMRegionDataMap* map) {
  if (map) {
    unsigned long x;
    for (x = 0; x < map->numRegions; x++)
      if (map->regions[x].data)
        free(map->regions[x].data);
    free(map);
  }
}
