// search_data.c, by Martin Michelsen, 2012
// library to support iteratively searching memory snapshots of a process

#include <stdlib.h>
#include <stdio.h>
#include "vmmap_data.h"
#include "search_data.h"

extern int* cancel_var;

////////////////////////////////////////////////////////////////////////////////
// byteswap routines

static inline void bswap_int8_t(void* a) { }
#define bswap_uint8_t bswap_int8_t

static inline void bswap_int16_t(void* a) {
  ((int8_t*)a)[0] ^= ((int8_t*)a)[1];
  ((int8_t*)a)[1] ^= ((int8_t*)a)[0];
  ((int8_t*)a)[0] ^= ((int8_t*)a)[1];
}
#define bswap_uint16_t bswap_int16_t

static inline void bswap_int32_t(void* a) {
  ((int8_t*)a)[0] ^= ((int8_t*)a)[3];
  ((int8_t*)a)[3] ^= ((int8_t*)a)[0];
  ((int8_t*)a)[0] ^= ((int8_t*)a)[3];
  ((int8_t*)a)[2] ^= ((int8_t*)a)[1];
  ((int8_t*)a)[1] ^= ((int8_t*)a)[2];
  ((int8_t*)a)[2] ^= ((int8_t*)a)[1];
}
#define bswap_uint32_t bswap_int32_t
#define bswap_float bswap_int32_t

static inline void bswap_int64_t(void* a) {
  ((int8_t*)a)[0] ^= ((int8_t*)a)[7];
  ((int8_t*)a)[7] ^= ((int8_t*)a)[0];
  ((int8_t*)a)[0] ^= ((int8_t*)a)[7];
  ((int8_t*)a)[6] ^= ((int8_t*)a)[1];
  ((int8_t*)a)[1] ^= ((int8_t*)a)[6];
  ((int8_t*)a)[6] ^= ((int8_t*)a)[1];
  ((int8_t*)a)[4] ^= ((int8_t*)a)[3];
  ((int8_t*)a)[3] ^= ((int8_t*)a)[4];
  ((int8_t*)a)[4] ^= ((int8_t*)a)[3];
  ((int8_t*)a)[2] ^= ((int8_t*)a)[5];
  ((int8_t*)a)[5] ^= ((int8_t*)a)[2];
  ((int8_t*)a)[2] ^= ((int8_t*)a)[5];
}
#define bswap_uint64_t bswap_int64_t
#define bswap_double bswap_int64_t

void bswap(void* a, int size) {
  if (size == 1)
    bswap_int8_t(a);
  if (size == 2)
    bswap_int16_t(a);
  if (size == 4)
    bswap_int32_t(a);
  if (size == 8)
    bswap_int64_t(a);
}

////////////////////////////////////////////////////////////////////////////////
// operator declarations

#define DECLARE_OP_FUNCTION(type, op, name) \
  static int name(type* a, type* b, int size) { \
    return op; \
  }
#define DECLARE_LE_OP_FUNCTION(type, op, name) \
  static int name(type* a, type* b, int size) { \
    bswap_##type(a); \
    bswap_##type(b); \
    type rv = op; \
    bswap_##type(a); \
    bswap_##type(b); \
    return rv; \
  }

static int null_pred(void* a, void* b, int size) { return 1; }

#define FLAG_OP   (*a ^ *b) && !((*a ^ *b) & ((*a ^ *b) - 1))

#define DECLARE_OPS_FOR_TYPE(type) \
  DECLARE_OP_FUNCTION(type, *a < *b,  type##_lt) \
  DECLARE_OP_FUNCTION(type, *a > *b,  type##_gt) \
  DECLARE_OP_FUNCTION(type, *a <= *b, type##_le) \
  DECLARE_OP_FUNCTION(type, *a >= *b, type##_ge) \
  DECLARE_OP_FUNCTION(type, *a == *b, type##_eq) \
  DECLARE_OP_FUNCTION(type, *a != *b, type##_ne) \
  DECLARE_OP_FUNCTION(type, FLAG_OP,  type##_flag)
#define DECLARE_OPS_FOR_TYPE_NOFLAG(type) \
  DECLARE_OP_FUNCTION(type, *a < *b,  type##_lt) \
  DECLARE_OP_FUNCTION(type, *a > *b,  type##_gt) \
  DECLARE_OP_FUNCTION(type, *a <= *b, type##_le) \
  DECLARE_OP_FUNCTION(type, *a >= *b, type##_ge) \
  DECLARE_OP_FUNCTION(type, *a == *b, type##_eq) \
  DECLARE_OP_FUNCTION(type, *a != *b, type##_ne)
#define DECLARE_LE_OPS_FOR_TYPE(type) \
  DECLARE_LE_OP_FUNCTION(type, *a < *b,  type##_lt_r) \
  DECLARE_LE_OP_FUNCTION(type, *a > *b,  type##_gt_r) \
  DECLARE_LE_OP_FUNCTION(type, *a <= *b, type##_le_r) \
  DECLARE_LE_OP_FUNCTION(type, *a >= *b, type##_ge_r) \
  DECLARE_LE_OP_FUNCTION(type, *a == *b, type##_eq_r) \
  DECLARE_LE_OP_FUNCTION(type, *a != *b, type##_ne_r) \
  DECLARE_LE_OP_FUNCTION(type, FLAG_OP,  type##_flag_r)
#define DECLARE_LE_OPS_FOR_TYPE_NOFLAG(type) \
  DECLARE_LE_OP_FUNCTION(type, *a < *b,  type##_lt_r) \
  DECLARE_LE_OP_FUNCTION(type, *a > *b,  type##_gt_r) \
  DECLARE_LE_OP_FUNCTION(type, *a <= *b, type##_le_r) \
  DECLARE_LE_OP_FUNCTION(type, *a >= *b, type##_ge_r) \
  DECLARE_LE_OP_FUNCTION(type, *a == *b, type##_eq_r) \
  DECLARE_LE_OP_FUNCTION(type, *a != *b, type##_ne_r)

DECLARE_OPS_FOR_TYPE(uint8_t)
DECLARE_OPS_FOR_TYPE(uint16_t)
DECLARE_OPS_FOR_TYPE(uint32_t)
DECLARE_OPS_FOR_TYPE(uint64_t)
DECLARE_OPS_FOR_TYPE(int8_t)
DECLARE_OPS_FOR_TYPE(int16_t)
DECLARE_OPS_FOR_TYPE(int32_t)
DECLARE_OPS_FOR_TYPE(int64_t)
DECLARE_OPS_FOR_TYPE_NOFLAG(float)
DECLARE_OPS_FOR_TYPE_NOFLAG(double)

DECLARE_LE_OPS_FOR_TYPE(uint16_t)
DECLARE_LE_OPS_FOR_TYPE(uint32_t)
DECLARE_LE_OPS_FOR_TYPE(uint64_t)
DECLARE_LE_OPS_FOR_TYPE(int16_t)
DECLARE_LE_OPS_FOR_TYPE(int32_t)
DECLARE_LE_OPS_FOR_TYPE(int64_t)
DECLARE_LE_OPS_FOR_TYPE_NOFLAG(float)
DECLARE_LE_OPS_FOR_TYPE_NOFLAG(double)

// comparators for arbitrary data type
static int data_lt(void* a, void* b, int size) { return (memcmp(a, b, size) < 0); }
static int data_gt(void* a, void* b, int size) { return (memcmp(a, b, size) > 0); }
static int data_le(void* a, void* b, int size) { return (memcmp(a, b, size) <= 0); }
static int data_ge(void* a, void* b, int size) { return (memcmp(a, b, size) >= 0); }
static int data_eq(void* a, void* b, int size) { return !memcmp(a, b, size); }
static int data_ne(void* a, void* b, int size) { return !!memcmp(a, b, size); }

static int data_flag(void* a, void* b, int size) {
  int bit_found = 0;
  int x;
  for (x = 0; (x < size) && (bit_found < 2); x++) {
    char axb = *(char*)a ^ *(char*)b;
    if (axb && !(axb & (axb - 1)))
      bit_found++;
  }
  return (bit_found == 1);
}

#define INFIX_FUNCTION_POINTERS_FOR_TYPE(type) \
  {(int (*)(void*, void*, int))type##_lt, \
   (int (*)(void*, void*, int))type##_gt, \
   (int (*)(void*, void*, int))type##_le, \
   (int (*)(void*, void*, int))type##_ge, \
   (int (*)(void*, void*, int))type##_eq, \
   (int (*)(void*, void*, int))type##_ne, \
   (int (*)(void*, void*, int))type##_flag, \
   (int (*)(void*, void*, int))null_pred}
#define INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(type) \
  {(int (*)(void*, void*, int))type##_lt, \
   (int (*)(void*, void*, int))type##_gt, \
   (int (*)(void*, void*, int))type##_le, \
   (int (*)(void*, void*, int))type##_ge, \
   (int (*)(void*, void*, int))type##_eq, \
   (int (*)(void*, void*, int))type##_ne, \
   NULL, \
   (int (*)(void*, void*, int))null_pred}
#define INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(type) \
  {(int (*)(void*, void*, int))type##_lt_r, \
   (int (*)(void*, void*, int))type##_gt_r, \
   (int (*)(void*, void*, int))type##_le_r, \
   (int (*)(void*, void*, int))type##_ge_r, \
   (int (*)(void*, void*, int))type##_eq_r, \
   (int (*)(void*, void*, int))type##_ne_r, \
   (int (*)(void*, void*, int))type##_flag_r, \
   (int (*)(void*, void*, int))null_pred}
#define INFIX_LE_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(type) \
  {(int (*)(void*, void*, int))type##_lt_r, \
   (int (*)(void*, void*, int))type##_gt_r, \
   (int (*)(void*, void*, int))type##_le_r, \
   (int (*)(void*, void*, int))type##_ge_r, \
   (int (*)(void*, void*, int))type##_eq_r, \
   (int (*)(void*, void*, int))type##_ne_r, \
   NULL, \
   (int (*)(void*, void*, int))null_pred}

struct _SearchTypeConfig {
  int (*funcs[8])(void* a, void* b, int size);
  int field_size;
} typeConfigs[19] = {
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint8_t),          1},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint16_t),         2},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(uint16_t),      2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint32_t),         4},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(uint32_t),      4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint64_t),         8},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(uint64_t),      8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int8_t),           1},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int16_t),          2},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(int16_t),       2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int32_t),          4},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(int32_t),       4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int64_t),          8},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE(int64_t),       8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(float),     4},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(float),  4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(double),    8},
  {INFIX_LE_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(double), 8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(data),             1},
};



// opens a new search given the search type
MemorySearchData* CreateNewSearch(int type, const char* name, long flags) {

  // alloc a new search object
  MemorySearchData* s = (MemorySearchData*)malloc(sizeof(MemorySearchData));
  if (!s)
    return NULL;

  strncpy(s->name, name, 0x80);
  s->searchflags = flags;
  s->prev_size = 0;
  s->type = type;
  s->numResults = 0;
  s->memory = NULL;
  return s;
}

// opens a new search given the search type name
MemorySearchData* CreateNewSearchByTypeName(const char* type, const char* name,
                                            long flags) {
  int t = GetSearchTypeByName(type);
  if (t == SEARCHTYPE_UNKNOWN)
    return NULL;
  return CreateNewSearch(t, name, flags);
}

// deletes a search data set
void DeleteSearch(MemorySearchData* search) {
  if (search) {
    if (search->memory)
      DestroyDataMap(search->memory);
    free(search);
  }
}

// byteswaps a value, for use in ApplyMapToSearch
void _ApplyMapToSearch_ByteswapValueIfLE(int searchtype, void* value) {

  if (!value)
    return;

  if (searchtype == SEARCHTYPE_UINT16_LE || searchtype == SEARCHTYPE_INT16_LE)
    bswap_int16_t(value);

  else if (searchtype == SEARCHTYPE_UINT32_LE ||
           searchtype == SEARCHTYPE_INT32_LE ||
           searchtype == SEARCHTYPE_FLOAT_LE)
    bswap_int32_t(value);

  else if (searchtype == SEARCHTYPE_UINT64_LE ||
           searchtype == SEARCHTYPE_INT64_LE ||
           searchtype == SEARCHTYPE_DOUBLE_LE)
    bswap_int64_t(value);
}

// copies a memory search data structure
// note that ApplyMapToSearch may modify the value
MemorySearchData* ApplyMapToSearch(MemorySearchData* s, VMRegionDataMap* map,
    int pred, void* value, unsigned long long size) {

  int x;
  _ApplyMapToSearch_ByteswapValueIfLE(s->type, value);

  // ensure that the input search and predicate are valid
  if (s->type < 0 || s->type >= SEARCHTYPE_UNKNOWN) {
    printf("invalid search type\n");
    return NULL;
  }
  if (pred < 0 || pred >= PRED_UNKNOWN) {
    printf("invalid predicate\n");
    return NULL;
  }
  if (!typeConfigs[s->type].funcs[pred]) {
    printf("predicate not valid for type\n");
    return NULL;
  }

  // alloc a new search object
  MemorySearchData* n = (MemorySearchData*)malloc(sizeof(MemorySearchData) +
                              s->numResults * sizeof(unsigned long long));
  if (!n) {
    printf("couldn\'t allocate new result set\n");
    return NULL;
  }

  // copy into the new object
  memcpy(n, s, sizeof(MemorySearchData) +
         s->numResults * sizeof(unsigned long long));
  n->memory = map;

  // if this is the first search, we need to check every address
  if (!s->memory) {

    // there is no previous data: if value is NULL, then pred better be the null
    // predicate
    if (!value && (pred != PRED_NULL)) {
      printf("non-null predicate and null value on first search\n");
      return NULL;
    }

    // there is no previous search: if this is a data search, then size better
    // not be zero
    if ((n->type == SEARCHTYPE_DATA) && !size) {
      printf("zero size given on initial data search\n");
      return NULL;
    }

    // check every address in every region
    unsigned long long y;
    unsigned long long numAllocatedResults = 0;
    int cont = 1;
    cancel_var = &cont;
    for (x = 0; cont && (x < n->memory->numRegions); x++) {

      // if the region has no data, skip it
      if (!n->memory->regions[x].data)
        continue;

      // for every cell in the region...
      for (y = 0;
           cont && (y < n->memory->regions[x].region._size - size);
           y += typeConfigs[n->type].field_size) {

        // do the comparison; skip it if it fails
        if (!typeConfigs[n->type].funcs[pred](&n->memory->regions[x].u8_data[y],
                                              value, size))
          continue;

        // expand the result list if necessary
        while (n->numResults >= numAllocatedResults) {
          if (!numAllocatedResults)
            numAllocatedResults = 128;
          else
            numAllocatedResults *= 2;
          VMRegionDataMap* old_mem = n->memory;
          n = (MemorySearchData*)realloc(n, sizeof(MemorySearchData) +
              numAllocatedResults * sizeof(unsigned long long));
          if (!n) {
            if (old_mem)
              DestroyDataMap(old_mem);
            printf("out of memory for result set expansion\n");
            return NULL;
          }
        }

        // add this result to the list
        n->results[n->numResults] = n->memory->regions[x].region._address + y;
        n->numResults++;
      }
    }
    cancel_var = NULL;

    // if we were canceled, return NULL
    if (!cont) {
      DeleteSearch(n);
      return NULL;
    }

  // this isn't the first search: we need to verify the results
  } else {

    // for each search result, run comparison and if it fails, delete the result
    // by setting its address to 0
    int x;
    int currentRegion = 0;
    int cont = 1;
    cancel_var = &cont;
    for (x = 0; cont && (x < n->numResults); x++) {

      // move the current region until we're inside it
      while (currentRegion < n->memory->numRegions &&
             n->memory->regions[currentRegion].region._address <= n->results[x] &&
             !VMAddressInRegion(n->memory->regions[currentRegion].region,
                                n->results[x]))
        currentRegion++;

      // if there are no regions left or the current region is after the current
      // result, then the current result is invalid - get rid of it
      if (currentRegion >= n->memory->numRegions ||
          n->memory->regions[currentRegion].region._address > n->results[x]) {
        n->results[x] = 0;
        continue;
      }

      // compare the current result and delete it if necessary
      void* current_data =
        (void*)(n->results[x] - n->memory->regions[currentRegion].region._address +
         (unsigned long long)n->memory->regions[currentRegion].data);
      void* target_data = (value && s->memory) ? value :
        (void*)(n->results[x] - s->memory->regions[currentRegion].region._address +
         (unsigned long long)s->memory->regions[currentRegion].data);
      if (!(typeConfigs[n->type].funcs[pred](current_data, target_data,
                                             size ? size : n->prev_size)))
        n->results[x] = 0;
    }

    // remove deleted results
    int numRemainingResults = 0;
    for (x = 0; x < n->numResults; x++) {
      if (n->results[x]) {
        n->results[numRemainingResults] = n->results[x];
        numRemainingResults++;
      }
    }
    n->numResults = numRemainingResults;
    cancel_var = NULL;
    
    // if we were canceled, return NULL
    if (!cont) {
      DeleteSearch(n);
      return NULL;
    }
    
  }

  // save the prev size and return
  n->prev_size = size;
  return (MemorySearchData*)realloc(n, sizeof(MemorySearchData) +
                                    n->numResults * sizeof(unsigned long long));
}

// deletes results from the search
int DeleteResults(MemorySearchData* s, uint64_t start, uint64_t end) {
  if (!s)
    return (-1);

  int startIndex, endIndex;
  for (startIndex = 0; (startIndex < s->numResults) &&
       (s->results[startIndex] < start); startIndex++);
  for (endIndex = startIndex; (endIndex < s->numResults) &&
       (s->results[endIndex] < end); endIndex++);
  if (endIndex == startIndex)
    return 0;

  int numToRemove = endIndex - startIndex;
  s->numResults -= numToRemove;
  memcpy(&s->results[startIndex], &s->results[endIndex],
         (s->numResults - startIndex) * sizeof(uint64_t));
  return numToRemove;
}

// returns a string representing the given search type name
const char* GetSearchTypeName(int type) {
  switch (type) {
    case SEARCHTYPE_UINT8:     return "u8";
    case SEARCHTYPE_UINT16:    return "u16";
    case SEARCHTYPE_UINT16_LE: return "lu16";
    case SEARCHTYPE_UINT32:    return "u32";
    case SEARCHTYPE_UINT32_LE: return "lu32";
    case SEARCHTYPE_UINT64:    return "u64";
    case SEARCHTYPE_UINT64_LE: return "lu64";
    case SEARCHTYPE_INT8:      return "s8";
    case SEARCHTYPE_INT16:     return "s16";
    case SEARCHTYPE_INT16_LE:  return "ls16";
    case SEARCHTYPE_INT32:     return "s32";
    case SEARCHTYPE_INT32_LE:  return "ls32";
    case SEARCHTYPE_INT64:     return "s64";
    case SEARCHTYPE_INT64_LE:  return "ls64";
    case SEARCHTYPE_FLOAT:     return "float";
    case SEARCHTYPE_FLOAT_LE:  return "lfloat";
    case SEARCHTYPE_DOUBLE:    return "double";
    case SEARCHTYPE_DOUBLE_LE: return "ldouble";
    case SEARCHTYPE_DATA:      return "arbitrary";
  }
  return "unknown";
}

// returns the search type associated with the given string
int GetSearchTypeByName(const char* name) {
  // skip whitespace
  while (name[0] == ' ' || name[0] == '\t')
    name++;

  if (name[0] == 'a')
    return SEARCHTYPE_DATA;

  // l = reverse-endian
  if (name[0] == 'l') {
    name++;

    // 'f' or 'd' means a floating-point type
    if (name[0] == 'f')
      return SEARCHTYPE_FLOAT_LE;
    if (name[0] == 'd')
      return SEARCHTYPE_DOUBLE_LE;

    // 'u' means unsigned
    if (name[0] == 'u') {
      switch (atoi(&name[1])) {
        case 8:  return SEARCHTYPE_UINT8;
        case 16: return SEARCHTYPE_UINT16_LE;
        case 32: return SEARCHTYPE_UINT32_LE;
        case 64: return SEARCHTYPE_UINT64_LE;
        default: return SEARCHTYPE_UNKNOWN;
      }
    }

    // assume type is signed by default
    if (name[0] == 's')
      name++;
    switch (atoi(&name[0])) {
      case 8:  return SEARCHTYPE_INT8;
      case 16: return SEARCHTYPE_INT16_LE;
      case 32: return SEARCHTYPE_INT32_LE;
      case 64: return SEARCHTYPE_INT64_LE;
    }

  // native-endian
  } else {

    // 'f' or 'd' means a floating-point type
    if (name[0] == 'f')
      return SEARCHTYPE_FLOAT;
    if (name[0] == 'd')
      return SEARCHTYPE_DOUBLE;
    
    // 'u' means unsigned
    if (name[0] == 'u') {
      switch (atoi(&name[1])) {
        case 8:  return SEARCHTYPE_UINT8;
        case 16: return SEARCHTYPE_UINT16;
        case 32: return SEARCHTYPE_UINT32;
        case 64: return SEARCHTYPE_UINT64;
        default: return SEARCHTYPE_UNKNOWN;
      }
    }
    
    // assume type is signed by default
    if (name[0] == 's')
      name++;
    switch (atoi(&name[0])) {
      case 8:  return SEARCHTYPE_INT8;
      case 16: return SEARCHTYPE_INT16;
      case 32: return SEARCHTYPE_INT32;
      case 64: return SEARCHTYPE_INT64;
    }
  }
    
  return SEARCHTYPE_UNKNOWN;
}

// returns a string representing the given predicate name
const char* GetPredicateName(int pred) {
  switch (pred) {
    case PRED_LESS:             return "<";
    case PRED_GREATER:          return ">";
    case PRED_LESS_OR_EQUAL:    return "<=";
    case PRED_GREATER_OR_EQUAL: return ">=";
    case PRED_EQUAL:            return "=";
    case PRED_NOT_EQUAL:        return "!=";
    case PRED_FLAG:             return "$";
    case PRED_NULL:             return "null";
  }
  return "unknown";
}

// returns the predicate associated with the given string
int GetPredicateByName(const char* name) {
  // skip whitespace
  while (name[0] == ' ' || name[0] == '\t')
    name++;

  // check for greater/less predicates
  if (name[0] == '>') {
    if (name[1] == '=')
      return PRED_GREATER_OR_EQUAL;
    return PRED_GREATER;
  }
  if (name[0] == '<') {
    if (name[1] == '=')
      return PRED_LESS_OR_EQUAL;
    return PRED_LESS;
  }

  // check for equal-type opers
  // we'll allow => and =< and =!, even though they're nonstandard
  if (name[0] == '=') {
    if (name[1] == '>')
      return PRED_GREATER_OR_EQUAL;
    else if (name[1] == '<')
      return PRED_LESS_OR_EQUAL;
    else if (name[1] == '!')
      return PRED_NOT_EQUAL;
    return PRED_EQUAL;
  }

  // check for !=
  if (name[0] == '!' && name[1] == '=')
    return PRED_NOT_EQUAL;

  // check for flag search
  if (name[0] == '$')
    return PRED_FLAG;

  // check for null predicate
  if (name[0] == 'n')
    return PRED_NULL;

  // no more predicates to check
  return PRED_UNKNOWN;
}

// returns 1 if the given type is an integral search type
int IsIntegerSearchType(int type) {
  return (type >= 0) && (type < SEARCHTYPE_FLOAT);
}

// returns 1 if the given type is a reverse-endian search type
int IsReverseEndianSearchType(int type) {
  return (type == SEARCHTYPE_UINT16_LE) ||
         (type == SEARCHTYPE_UINT32_LE) ||
         (type == SEARCHTYPE_UINT64_LE) ||
         (type == SEARCHTYPE_INT16_LE) ||
         (type == SEARCHTYPE_INT32_LE) ||
         (type == SEARCHTYPE_INT64_LE) ||
         (type == SEARCHTYPE_FLOAT_LE) ||
         (type == SEARCHTYPE_DOUBLE_LE);
}

// returns the size, in bytes, of the search data type
// 0 for arbitrary data searches
int SearchDataSize(int type) {

  switch (type) {
    case SEARCHTYPE_UINT8:
    case SEARCHTYPE_INT8:
      return 1;

    case SEARCHTYPE_UINT16:
    case SEARCHTYPE_UINT16_LE:
    case SEARCHTYPE_INT16:
    case SEARCHTYPE_INT16_LE:
      return 2;

    case SEARCHTYPE_UINT32:
    case SEARCHTYPE_UINT32_LE:
    case SEARCHTYPE_INT32:
    case SEARCHTYPE_INT32_LE:
    case SEARCHTYPE_FLOAT:
    case SEARCHTYPE_FLOAT_LE:
      return 4;

    case SEARCHTYPE_UINT64:
    case SEARCHTYPE_UINT64_LE:
    case SEARCHTYPE_INT64:
    case SEARCHTYPE_INT64_LE:
    case SEARCHTYPE_DOUBLE:
    case SEARCHTYPE_DOUBLE_LE:
      return 8;
  }

  return 0;
}
