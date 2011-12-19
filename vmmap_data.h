#ifndef VMMAP_DATA_H
#define VMMAP_DATA_H

#include "vmmap.h"

typedef struct _VMRegionData {
  VMRegion region;
  union {
    void*     data;
    int8_t*   s8_data;
    uint8_t*  u8_data;
    int16_t*  s16_data;
    uint16_t* u16_data;
    int32_t*  s32_data;
    uint32_t* u32_data;
    int64_t*  s64_data;
    uint64_t* u64_data;
    float*    float_data;
    double*   double_data;
  };
} VMRegionData;

typedef struct _VMRegionDataMap {
  pid_t process;
  unsigned long numRegions;
  VMRegionData regions[0];
} VMRegionDataMap;

VMRegionDataMap* DumpProcessMemory(pid_t pid, unsigned int modeMask);
VMRegionDataMap* GetProcessRegionList(pid_t pid, unsigned int modeMask);
int UpdateDataMap(VMRegionDataMap* map);
VMRegionDataMap* CopyDataMap(VMRegionDataMap* src);
void DestroyDataMap(VMRegionDataMap* map);

#endif // VMMAP_DATA_H
