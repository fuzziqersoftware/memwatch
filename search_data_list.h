// search_data_list.h, by Martin Michelsen, 2012
// simple collection of searches

#ifndef SEARCH_DATA_LIST_H
#define SEARCH_DATA_LIST_H

#include "search_data.h"

typedef struct _MemorySearchDataList {
  int num_searches;
  MemorySearchData** searches;
} MemorySearchDataList;

MemorySearchDataList* CreateSearchList();
void DeleteSearchList(MemorySearchDataList* list);

int AddSearchToList(MemorySearchDataList* list, MemorySearchData* data);
int DeleteSearchByName(MemorySearchDataList* list, const char* name);
MemorySearchData* GetSearchByName(MemorySearchDataList* list, const char* name);
MemorySearchData* CopySearch(MemorySearchDataList* l, const char* name,
    const char* new_name);

void PrintSearches(MemorySearchDataList* list);

#endif // SEARCH_DATA_LIST_H
