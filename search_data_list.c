#include <stdlib.h>
#include <stdio.h>
#include "search_data_list.h"

MemorySearchDataList* CreateSearchList() {
  MemorySearchDataList* list = (MemorySearchDataList*)malloc(
      sizeof(MemorySearchDataList));
  if (!list)
    return NULL;

  list->numSearches = 0;
  list->searches = NULL;
  return list;
}

void DeleteSearchList(MemorySearchDataList* list) {
  int x;
  if (list->searches) {
    for (x = 0; x < list->numSearches; x++)
      DeleteSearch(list->searches[x]);
    free(list->searches);
  }
  free(list);
}

int AddSearchToList(MemorySearchDataList* list, MemorySearchData* data) {

  if (!data->name || !data->name[0])
    return 0;

  int x;
  for (x = 0; x < list->numSearches; x++) {
    if (!strcmp(data->name, list->searches[x]->name)) {
      DeleteSearch(list->searches[x]);
      list->searches[x] = data;
      return 1;
    }
  }

  list->searches = (MemorySearchData**)realloc(list->searches,
      sizeof(MemorySearchData*) * (list->numSearches + 1));
  if (!list->searches) {
    list->numSearches = 0;
    return 0;
  }
  list->searches[list->numSearches] = data;
  list->numSearches++;

  return 1;
}

int DeleteSearchByName(MemorySearchDataList* list, const char* name) {

  int x;
  for (x = 0; x < list->numSearches; x++)
    if (!strcmp(name, list->searches[x]->name))
      break;

  if (x >= list->numSearches)
    return 0;

  DeleteSearch(list->searches[x]);
  list->numSearches--;
  memcpy(&list->searches[x], &list->searches[x + 1], sizeof(MemorySearchData*) *
         (list->numSearches - x));
  list->searches = (MemorySearchData**)realloc(list->searches,
      sizeof(MemorySearchData*) * list->numSearches);
  if (!list->searches) {
    list->numSearches = 0;
    return 0;
  }

  return 1;
}

MemorySearchData* GetSearchByName(MemorySearchDataList* list,
                                  const char* name) {

  int x;
  for (x = 0; x < list->numSearches; x++)
    if (!strcmp(name, list->searches[x]->name))
      return list->searches[x];

  return NULL;
}

void PrintSearches(MemorySearchDataList* list) {

  int x;
  for (x = 0; x < list->numSearches; x++) {
    if (!list->searches[x]->memory)
      printf("  %s (initial search not done)\n", list->searches[x]->name);
    else
      printf("  %s (%llu results)\n", list->searches[x]->name,
             list->searches[x]->numResults);
  }
  printf("total searches: %d\n", list->numSearches);
}