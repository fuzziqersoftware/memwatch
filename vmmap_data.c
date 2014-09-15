// vmmap_data.c, by Martin Michelsen, 2012
// process memory state capture functions

#include <stdlib.h>

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
  map->num_regions = 0;

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
                                  sizeof(VMRegionData) * (map->num_regions + 1));
    if (!map) {
      cancel_var = NULL;
      return NULL;
    }

    // fill in region info and advance
    map->regions[map->num_regions].region._process = pid;
    map->regions[map->num_regions].region._address = vmr._address;
    map->regions[map->num_regions].region._size = vmr._size;
    map->regions[map->num_regions].region._attributes = vmr._attributes;
    map->regions[map->num_regions].error = 0;
    map->regions[map->num_regions].data = NULL;
    map->num_regions++;
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

  int x;

  // first get the list of regions
  VMRegionDataMap* map = GetProcessRegionList(pid, modeMask);
  if (!map)
    return NULL;

  // now, for each region...
  int cont = 1;
  cancel_var = &cont;
  for (x = 0; cont && (x < map->num_regions); x++) {

    // alloc space for this region's data
    map->regions[x].data = VMAlloc(map->regions[x].region._size);
    if (!map->regions[x].data) {
      DestroyDataMap(map);
      cancel_var = NULL;
      return NULL;
    }

    // make the region readable if we have to
    if (!(map->regions[x].region._attributes & VMREGION_READABLE)) {
      map->regions[x].error = VMSetRegionProtection(pid,
          map->regions[x].region._address, map->regions[x].region._size,
          VMREGION_READABLE, VMREGION_READABLE);
    }

    // read the region's contents!
    mach_vm_size_t local_size = map->regions[x].region._size;
    map->regions[x].error = VMReadBytes(map->regions[x].region._process,
                        map->regions[x].region._address,
                        map->regions[x].data, &local_size);

    // if reading failed, don't include the region's data
    if (map->regions[x].error ||
        (map->regions[x].region._size != local_size)) {
      VMFree(map->regions[x].data, map->regions[x].region._size);
      map->regions[x].data = NULL;
    }

    // if we made the region readable, restore its previous protection
    if (!(map->regions[x].region._attributes & VMREGION_READABLE)) {
      map->regions[x].error = VMSetRegionProtection(pid,
          map->regions[x].region._address, map->regions[x].region._size,
          map->regions[x].region._attributes, VMREGION_ALL);
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

  // for each region...
  int cont = 1;
  cancel_var = &cont;
  for (x = 0; x < map->num_regions; x++) {

    // read the data in this section
    mach_vm_size_t newsize = map->regions[x].region._size;
    map->regions[x].error = VMReadBytes(map->process,
        map->regions[x].region._address, map->regions[x].data, &newsize);

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

VMRegionDataMap* CopyDataMap(VMRegionDataMap* s) {

  int object_size = sizeof(VMRegionDataMap) +
      sizeof(VMRegionData) * s->num_regions;
  VMRegionDataMap* n = (VMRegionDataMap*)malloc(object_size);
  if (!n)
    return NULL;
  memcpy(n, s, object_size);

  long x;
  for (x = 0; x < s->num_regions; x++) {
    if (!s->regions[x].data)
      continue; // no data for this region
    n->regions[x].data = malloc(n->regions[x].region._size);
    if (!n->regions[x].data)
      break;
    memcpy(n->regions[x].data, s->regions[x].data, n->regions[x].region._size);
  }

  // if the region copying didn't finish, then this is an incomplete map and we
  // should destroy it
  if (x < s->num_regions) {
    for (; x >= 0; x--)
      if (n->regions[x].data)
        free(n->regions[x].data);
    free(n);
    n = NULL;
  }

  return n;
}

void DestroyDataMap(VMRegionDataMap* map) {
  if (map) {
    unsigned long x;
    for (x = 0; x < map->num_regions; x++)
      if (map->regions[x].data)
        VMFree(map->regions[x].data, map->regions[x].region._size);
    free(map);
  }
}
