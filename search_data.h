// search_data.c, by Martin Michelsen, 2012
// library to support iteratively searching memory snapshots of a process

#ifndef SEARCH_DATA_H
#define SEARCH_DATA_H

#include <stdint.h>

#include "vmmap_data.h"

#define SEARCHTYPE_UINT8       0
#define SEARCHTYPE_UINT16      1
#define SEARCHTYPE_UINT16_LE   2
#define SEARCHTYPE_UINT32      3
#define SEARCHTYPE_UINT32_LE   4
#define SEARCHTYPE_UINT64      5
#define SEARCHTYPE_UINT64_LE   6
#define SEARCHTYPE_INT8        7
#define SEARCHTYPE_INT16       8
#define SEARCHTYPE_INT16_LE    9
#define SEARCHTYPE_INT32      10
#define SEARCHTYPE_INT32_LE   11
#define SEARCHTYPE_INT64      12
#define SEARCHTYPE_INT64_LE   13
#define SEARCHTYPE_FLOAT      14
#define SEARCHTYPE_FLOAT_LE   15
#define SEARCHTYPE_DOUBLE     16
#define SEARCHTYPE_DOUBLE_LE  17
#define SEARCHTYPE_DATA       18
#define SEARCHTYPE_UNKNOWN    19

#define PRED_LESS              0
#define PRED_GREATER           1
#define PRED_LESS_OR_EQUAL     2
#define PRED_GREATER_OR_EQUAL  3
#define PRED_EQUAL             4
#define PRED_NOT_EQUAL         5
#define PRED_FLAG              6
#define PRED_NULL              7
#define PRED_UNKNOWN           8

#define SEARCHFLAG_ALLMEMORY   1

/* possible states for MemorySearchData:
 * 
 * known value search: memory and results always valid after 1st search
 * unknown value search: memory always valid after 1st search,
 *                       results invalid until 2nd search
 * make results always valid: null predicate is always TRUE
 */

typedef struct _MemorySearchData {
  int type;
  long searchflags;
  char name[0x80];
  VMRegionDataMap* memory;
  uint64_t prev_size;
  uint64_t numResults;
  uint64_t results[0];
} MemorySearchData;

MemorySearchData* CreateNewSearch(int type, const char* name, long flags);
MemorySearchData* CreateNewSearchByTypeName(const char* type, const char* name,
                                            long flags);
void DeleteSearch(MemorySearchData* search);
MemorySearchData* CopySearchData(MemorySearchData* d);

// does the actual searching. ComparePredicate tells which comparator to use,
// and value is the value to compare with (NULL to use previous values)
// DON'T DELETE THE MAP AFTER CALLING THIS FUNCTION. it becomes part of the
// search object and will be deleted when the search object is deleted.
MemorySearchData* ApplyMapToSearch(MemorySearchData* s, VMRegionDataMap* map,
    int pred, void* value, unsigned long long size, uint64_t max_results);
int DeleteResults(MemorySearchData* s, uint64_t start, uint64_t end);

const char* GetSearchTypeName(int type);
int GetSearchTypeByName(const char* name);
const char* GetPredicateName(int pred);
int GetPredicateByName(const char* name);

int IsIntegerSearchType(int type);
int IsReverseEndianSearchType(int type);
int SearchDataSize(int type);

void bswap(void* a, int size);

#endif // SEARCH_DATA_H
