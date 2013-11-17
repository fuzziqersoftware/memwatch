// search_data_list.c, by Martin Michelsen, 2012
// simple collection of searches

#include <stdio.h>
#include <stdlib.h>

#include "search_data_list.h"

// create a list of searches
MemorySearchDataList* CreateSearchList() {

  // allocate space for the list struct and fill in initial values
  MemorySearchDataList* list = (MemorySearchDataList*)malloc(
      sizeof(MemorySearchDataList));
  if (!list)
    return NULL;

  list->numSearches = 0;
  list->searches = NULL;
  return list;
}

// delete a search list and all contained searches
void DeleteSearchList(MemorySearchDataList* list) {
  int x;
  if (list->searches) {
    for (x = 0; x < list->numSearches; x++)
      DeleteSearch(list->searches[x]);
    free(list->searches);
  }
  free(list);
}

// add a search to this list. any existing search with the same name will be
// deleted and replaced with the new search
int AddSearchToList(MemorySearchDataList* list, MemorySearchData* data) {

  // no name? can't add it to the list
  if (!data->name || !data->name[0])
    return 0;

  // see if there's a search with the same name; if so, replace it
  int x;
  for (x = 0; x < list->numSearches; x++) {
    if (!strcmp(data->name, list->searches[x]->name)) {
      DeleteSearch(list->searches[x]);
      list->searches[x] = data;
      return 1;
    }
  }

  // make room for this search in the searches list and add it to the list
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

// delete a search by name
int DeleteSearchByName(MemorySearchDataList* list, const char* name) {

  // find this search in the list by name
  int x;
  for (x = 0; x < list->numSearches; x++)
    if (!strcmp(name, list->searches[x]->name))
      break;

  // not found? just return
  if (x >= list->numSearches)
    return 0;

  // delete the search and remove its spot in the list
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

// find a search by name
MemorySearchData* GetSearchByName(MemorySearchDataList* list,
                                  const char* name) {

  int x;
  for (x = 0; x < list->numSearches; x++)
    if (!strcmp(name, list->searches[x]->name))
      return list->searches[x];

  return NULL;
}

// make a copy of an existing search
MemorySearchData* CopySearch(MemorySearchDataList* l, const char* name,
    const char* new_name) {

  MemorySearchData* orig_search = GetSearchByName(l, name);
  if (!orig_search)
    return NULL;

  MemorySearchData* new_search = CopySearchData(orig_search);
  if (!new_search)
    return NULL;

  strncpy(new_search->name, new_name, 0x80);
  AddSearchToList(l, new_search);
  return new_search;
}

// print a list of the searches in this list
void PrintSearches(MemorySearchDataList* list) {

  int x;
  for (x = 0; x < list->numSearches; x++) {
    printf("  %s %s ", GetSearchTypeName(list->searches[x]->type),
           list->searches[x]->name);
    if (!list->searches[x]->memory)
      printf("(initial search not done)");
    else
      printf("(%llu results)", list->searches[x]->numResults);
    if (list->searches[x]->searchflags & SEARCHFLAG_ALLMEMORY)
      printf(" (all memory)");
    printf("\n");
  }
  printf("total searches: %d\n", list->numSearches);
}
