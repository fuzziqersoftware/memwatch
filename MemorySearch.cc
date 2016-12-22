#include "MemorySearch.hh"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

using namespace std;


////////////////////////////////////////////////////////////////////////////////
// operator declarations

// this section declares comparator functions, one for each pair of (operation,
// datatype), as well as the null predicate (which is always true).

static bool null_pred(void* a, void* b, size_t size) {
  return true;
}

#define DECLARE_OP_FUNCTION(type, op, name) \
  static bool name(const type* a, const type* b, size_t size) { \
    return *a op *b; \
  }
#define DECLARE_INT_OP_FUNCTION_RE(type, op, width, name) \
  static bool name(const type* a, const type* b, size_t size) { \
    return bswap##width(*a) op bswap##width(*b); \
  }
#define DECLARE_FLOAT_OP_FUNCTION_RE(type, op, width, name) \
  static bool name(const type* a, const type* b, size_t size) { \
    type ar, br; \
    *(uint##width##_t*)&ar = bswap##width(*(uint##width##_t*)a); \
    *(uint##width##_t*)&br = bswap##width(*(uint##width##_t*)b); \
    return ar op br; \
  }

#define DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(type, width) \
  DECLARE_OP_FUNCTION(type, <,  type##_lt) \
  DECLARE_OP_FUNCTION(type, >,  type##_gt) \
  DECLARE_OP_FUNCTION(type, <=, type##_le) \
  DECLARE_OP_FUNCTION(type, >=, type##_ge) \
  DECLARE_OP_FUNCTION(type, ==, type##_eq) \
  DECLARE_OP_FUNCTION(type, !=, type##_ne)

#define DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(type, width) \
  DECLARE_INT_OP_FUNCTION_RE(type, <,  width, type##_lt_r) \
  DECLARE_INT_OP_FUNCTION_RE(type, >,  width, type##_gt_r) \
  DECLARE_INT_OP_FUNCTION_RE(type, <=, width, type##_le_r) \
  DECLARE_INT_OP_FUNCTION_RE(type, >=, width, type##_ge_r) \
  DECLARE_INT_OP_FUNCTION_RE(type, ==, width, type##_eq_r) \
  DECLARE_INT_OP_FUNCTION_RE(type, !=, width, type##_ne_r)

#define DECLARE_FLOAT_OP_FUNCTIONS_FOR_TYPE(type, width) \
  DECLARE_OP_FUNCTION(type, <,  type##_lt) \
  DECLARE_OP_FUNCTION(type, >,  type##_gt) \
  DECLARE_OP_FUNCTION(type, <=, type##_le) \
  DECLARE_OP_FUNCTION(type, >=, type##_ge) \
  DECLARE_OP_FUNCTION(type, ==, type##_eq) \
  DECLARE_OP_FUNCTION(type, !=, type##_ne) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, <,  width, type##_lt_r) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, >,  width, type##_gt_r) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, <=, width, type##_le_r) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, >=, width, type##_ge_r) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, ==, width, type##_eq_r) \
  DECLARE_FLOAT_OP_FUNCTION_RE(type, !=, width, type##_ne_r)

DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(uint8_t, 8)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(uint16_t, 16)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(uint32_t, 32)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(uint64_t, 64)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(int8_t, 8)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(int16_t, 16)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(int32_t, 32)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE(int64_t, 64)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(uint16_t, 16)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(uint32_t, 32)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(uint64_t, 64)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(int16_t, 16)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(int32_t, 32)
DECLARE_INT_OP_FUNCTIONS_FOR_TYPE_RE(int64_t, 64)
DECLARE_FLOAT_OP_FUNCTIONS_FOR_TYPE(float, 32)
DECLARE_FLOAT_OP_FUNCTIONS_FOR_TYPE(double, 64)

#define DECLARE_FLAG_OP_FUNCTION(type) \
  static bool type##_flag(type* a, type* b, size_t size) { \
    return (*a ^ *b) && !((*a ^ *b) & ((*a ^ *b) - 1)); \
  }

DECLARE_FLAG_OP_FUNCTION(uint8_t)
DECLARE_FLAG_OP_FUNCTION(uint16_t)
DECLARE_FLAG_OP_FUNCTION(uint32_t)
DECLARE_FLAG_OP_FUNCTION(uint64_t)
DECLARE_FLAG_OP_FUNCTION(int8_t)
DECLARE_FLAG_OP_FUNCTION(int16_t)
DECLARE_FLAG_OP_FUNCTION(int32_t)
DECLARE_FLAG_OP_FUNCTION(int64_t)

// comparators for arbitrary data type. these aren't as simple as the above
// since items of the 'data' type can be an arbitrary size.
#define DECLARE_DATA_OP_FUNCTION(op, name) \
  static bool name(void* a, void* b, size_t size) { \
    return memcmp(a, b, size) op 0; \
  }

DECLARE_DATA_OP_FUNCTION(<, data_lt)
DECLARE_DATA_OP_FUNCTION(>, data_gt)
DECLARE_DATA_OP_FUNCTION(<=, data_le)
DECLARE_DATA_OP_FUNCTION(>=, data_ge)
DECLARE_DATA_OP_FUNCTION(==, data_eq)
DECLARE_DATA_OP_FUNCTION(!=, data_ne)

static bool data_flag(void* a, void* b, size_t size) {
  int bit_found = 0;
  size_t x;
  for (x = 0; (x < size) && (bit_found < 2); x++) {
    char axb = *(char*)a ^ *(char*)b;
    if (axb && !(axb & (axb - 1))) {
      bit_found++;
    }
  }
  return (bit_found == 1);
}

// these macros allow us to more easily specify the list of function pointers
// for each type, to evaluate predicates on that type
#define INFIX_FUNCTION_POINTERS_FOR_TYPE(type) \
  {(bool (*)(const void*, const void*, size_t))type##_lt, \
   (bool (*)(const void*, const void*, size_t))type##_gt, \
   (bool (*)(const void*, const void*, size_t))type##_le, \
   (bool (*)(const void*, const void*, size_t))type##_ge, \
   (bool (*)(const void*, const void*, size_t))type##_eq, \
   (bool (*)(const void*, const void*, size_t))type##_ne, \
   (bool (*)(const void*, const void*, size_t))type##_flag, \
   (bool (*)(const void*, const void*, size_t))null_pred}
#define INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(type) \
  {(bool (*)(const void*, const void*, size_t))type##_lt, \
   (bool (*)(const void*, const void*, size_t))type##_gt, \
   (bool (*)(const void*, const void*, size_t))type##_le, \
   (bool (*)(const void*, const void*, size_t))type##_ge, \
   (bool (*)(const void*, const void*, size_t))type##_eq, \
   (bool (*)(const void*, const void*, size_t))type##_ne, \
   NULL, \
   (bool (*)(const void*, const void*, size_t))null_pred}
#define INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(type) \
  {(bool (*)(const void*, const void*, size_t))type##_lt_r, \
   (bool (*)(const void*, const void*, size_t))type##_gt_r, \
   (bool (*)(const void*, const void*, size_t))type##_le_r, \
   (bool (*)(const void*, const void*, size_t))type##_ge_r, \
   (bool (*)(const void*, const void*, size_t))type##_eq_r, \
   (bool (*)(const void*, const void*, size_t))type##_ne_r, \
   (bool (*)(const void*, const void*, size_t))type##_flag, \
   (bool (*)(const void*, const void*, size_t))null_pred}
#define INFIX_FUNCTION_POINTERS_FOR_TYPE_RE_NOFLAG(type) \
  {(bool (*)(const void*, const void*, size_t))type##_lt_r, \
   (bool (*)(const void*, const void*, size_t))type##_gt_r, \
   (bool (*)(const void*, const void*, size_t))type##_le_r, \
   (bool (*)(const void*, const void*, size_t))type##_ge_r, \
   (bool (*)(const void*, const void*, size_t))type##_eq_r, \
   (bool (*)(const void*, const void*, size_t))type##_ne_r, \
   NULL, \
   (bool (*)(const void*, const void*, size_t))null_pred}

struct SearchTypeConfig {
  bool (*evaluators[8])(const void* a, const void* b, size_t size);
  size_t field_size;
};

static const vector<SearchTypeConfig> search_type_configs({
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint8_t),          1},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint16_t),         2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(uint16_t),      2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint32_t),         4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(uint32_t),      4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(uint64_t),         8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(uint64_t),      8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int8_t),           1},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int16_t),          2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(int16_t),       2},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int32_t),          4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(int32_t),       4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(int64_t),          8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE(int64_t),       8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(float),     4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE_NOFLAG(float),  4},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_NOFLAG(double),    8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE_RE_NOFLAG(double), 8},
  {INFIX_FUNCTION_POINTERS_FOR_TYPE(data),             0},
});



MemorySearch::MemorySearch(MemorySearch::Type type, bool all_memory) :
    type(type), results_valid(false), all_memory(all_memory), memory(),
    prev_size(0), results() { }

MemorySearch::Type MemorySearch::get_type() const {
  return this->type;
}

bool MemorySearch::has_memory() const {
  return (bool)this->memory;
}

bool MemorySearch::has_valid_results() const {
  return this->results_valid;
}

bool MemorySearch::is_all_memory() const {
  return this->all_memory;
}

const vector<uint64_t>& MemorySearch::get_results() const {
  return this->results;
}

void MemorySearch::check_can_update(Predicate predicate, const string& data_c) {
  // make sure the predicate has an evaluator (e.g. can't do a flag search on a
  // float)
  const auto& type_config = search_type_configs.at((size_t)this->type);
  if (!type_config.evaluators[(size_t)predicate]) {
    throw invalid_argument("predicate not valid for search type");
  }

  // if it's a fixed-size search, make sure the value is the right size or is
  // missing (for unknown-value searches)
  if ((type != Type::DATA) && !data_c.empty() &&
      (data_c.size() != this->value_size_for_search_type(this->type))) {
    throw invalid_argument("provided data has incorrect size");
  }

  if (!this->memory) {
    // there is no previous data: we can only search if the predicate is ANY or
    // a value is given
    if (data_c.empty() && (predicate != Predicate::ALL)) {
      throw invalid_argument("initial searches must have a value or null predicate");
    }

    // there is no previous search: if this is a data search, then size can't be
    // zero
    if ((this->type == Type::DATA) && data_c.empty()) {
      throw invalid_argument("initial data search must have nonzero size");
    }
  }
}

void MemorySearch::update(shared_ptr<vector<ProcessMemoryAdapter::Region>> new_memory,
    Predicate predicate, const string& data_c, size_t max_results) {

  this->check_can_update(predicate, data_c);

  const auto& type_config = search_type_configs.at((size_t)this->type);
  string data = data_c;

  auto evaluator = type_config.evaluators[(size_t)predicate];

  // if this is the first search or the results are invalid, we need to check
  // every address (the results can be invalid even if memory is valid if the
  // initial search was an unknown-value search)

  // if this is the first search for an unknown-value search, don't iterate at
  // all - just save the memory for use in the next search
  if (!this->memory && (predicate == Predicate::ALL)) {
    this->memory = new_memory;
    return;

  // if this is the first search of a known-value search, evaluate the predicate
  // for all cells in all regions
  } else if (!this->memory) {

    for (const auto& region : *new_memory) {

      // if the region has no data, skip it
      if (region.data.empty()) {
        continue;
      }

      // for every cell in the region...
      for (uint64_t y = 0; (y < region.size - data.size()) && (this->results.size() < max_results); y += type_config.field_size) {

        // do the comparison; skip it if it fails
        if (!evaluator(region.data.data() + y, data.data(), data.size())) {
          continue;
        }

        // this address is a match - add this result to the list
        this->results.emplace_back(region.addr + y);

        // if there's a max and we've reached it, stop searching
        if (this->results.size() >= max_results) {
          break;
        }
      }
    }
    this->results_valid = true;

  // if this is the second search on an unknown initial value search, evaluate
  // the predicate comparing the data to the previous region set. note that we
  // support, but don't optimize for, the case where data is given here - this
  // case is a misuse of the program, since it renders the initial ALL predicate
  // search useless.
  } else if (!this->results_valid) {

    // for each cell, run comparison and add it to the result list if the
    // comparison succeeds
    size_t old_region_index = 0;
    for (const auto& new_region : *new_memory) {

      // scan over all of new_region and run comparisons where we can
      for (uint64_t y = 0; y < new_region.size; y += type_config.field_size) {

        // if this region has no data, skip it
        if (new_region.data.empty()) {
          continue;
        }

        // move the current old region until it's not entirely before the
        // current address, skipping old regions that have no data
        while ((old_region_index < this->memory->size()) &&
            ((*this->memory)[old_region_index].data.empty() ||
             ((*this->memory)[old_region_index].end_addr() <= new_region.addr + y))) {
          old_region_index++;
        }

        // if there are no old regions left, we're done
        if (old_region_index >= this->memory->size()) {
          break;
        }

        // if the current address is outside of old_region, then old_region must
        // start after the current address - skip ahead appropriately
        const auto& old_region = (*this->memory)[old_region_index];
        if (new_region.addr + y < old_region.addr) {
          y = old_region.addr - new_region.addr - type_config.field_size;
          continue;
        }
        uint64_t old_region_y = new_region.addr + y - old_region.addr;

        // compare the current cell and add it to the results if we succeed
        const void* current_data = (const void*)(y + (uint64_t)new_region.data.data());
        const void* target_data = data.empty() ? (const void*)(old_region_y + (uint64_t)old_region.data.data()) : data.data();
        if (evaluator(current_data, target_data, data.empty() ? this->prev_size : data.size())) {
          results.emplace_back(new_region.addr + y);
        }
      }
    }
    this->results_valid = true;

  // this isn't the first search: we just need to verify the existing results
  } else {

    // for each search result, run comparison and if it fails, delete the result
    // by setting its address to 0. we'll filter out the nulls at the end
    size_t old_region_index = 0;
    size_t new_region_index = 0;
    size_t num_outside_regions = 0;
    size_t num_inside_bad_regions = 0;
    for (auto& result : this->results) {

      // move the current region until we're inside it, for both maps
      while ((new_region_index < new_memory->size()) &&
             ((*new_memory)[new_region_index].end_addr() <= result)) {
        new_region_index++;
      }
      while ((old_region_index < this->memory->size()) &&
             ((*this->memory)[old_region_index].end_addr() <= result)) {
        old_region_index++;
      }

      // if there are no regions left, then the current result is invalid
      if (new_region_index >= new_memory->size() ||
          old_region_index >= this->memory->size()) {
        result = 0;
        num_outside_regions++;
        continue;
      }

      // if the current region is after the current result, then the current
      // result is invalid
      const auto& new_region = (*new_memory)[new_region_index];
      const auto& old_region = (*this->memory)[old_region_index];
      if (new_region.addr > result || old_region.addr > result) {
        result = 0;
        num_outside_regions++;
        continue;
      }

      // now the current result is within the current region - but if the
      // current region has no data (couldn't be read for some reason?) then
      // we'll have to delete this result
      if (new_region.data.empty()) {
        result = 0;
        num_inside_bad_regions++;
        continue;
      }

      // compare the current result and delete it if necessary
      const void* current_data = (const void*)(result - new_region.addr + (uint64_t)new_region.data.data());
      const void* target_data = data.empty() ? (const void*)(result - old_region.addr + (uint64_t)old_region.data.data()) : data.data();
      if (!evaluator(current_data, target_data, data.empty() ? this->prev_size : data.size())) {
        result = 0;
      }
    }

    // remove deleted results
    size_t num_results = 0;
    for (size_t x = 0; x < this->results.size(); x++) {
      if (this->results[x]) {
        this->results[num_results] = this->results[x];
        num_results++;
      }
    }
    this->results.resize(num_results);

    // TODO: return num_inside_bad_regions and num_outside_regions somehow
  }

  // save the prev size and memory contents
  this->prev_size = data.size();
  this->memory = new_memory;
}

void MemorySearch::delete_results(uint64_t start, uint64_t end) {
  auto start_it = lower_bound(this->results.begin(), this->results.end(), start);
  auto end_it = lower_bound(this->results.begin(), this->results.end(), end);
  this->results.erase(start_it, end_it);
}

// returns a string representing the given search type name
const char* MemorySearch::name_for_search_type(MemorySearch::Type type) {
  switch (type) {
    case Type::UINT8:     return "uint8";
    case Type::UINT16:    return "uint16";
    case Type::UINT16_RE: return "ruint16";
    case Type::UINT32:    return "uint32";
    case Type::UINT32_RE: return "ruint32";
    case Type::UINT64:    return "uint64";
    case Type::UINT64_RE: return "ruint64";
    case Type::INT8:      return "int8";
    case Type::INT16:     return "int16";
    case Type::INT16_RE:  return "rint16";
    case Type::INT32:     return "int32";
    case Type::INT32_RE:  return "rint32";
    case Type::INT64:     return "int64";
    case Type::INT64_RE:  return "rint64";
    case Type::FLOAT:     return "float";
    case Type::FLOAT_RE:  return "rfloat";
    case Type::DOUBLE:    return "double";
    case Type::DOUBLE_RE: return "rdouble";
    case Type::DATA:      return "string";
  }
  return "unknown";
}

static const unordered_map<string, MemorySearch::Type> search_type_names = {
  {"u8",        MemorySearch::Type::UINT8},
  {"uint8",     MemorySearch::Type::UINT8},
  {"uint8_t",   MemorySearch::Type::UINT8},
  {"u16",       MemorySearch::Type::UINT16},
  {"uint16",    MemorySearch::Type::UINT16},
  {"uint16_t",  MemorySearch::Type::UINT16},
  {"ru16",      MemorySearch::Type::UINT16_RE},
  {"ruint16",   MemorySearch::Type::UINT16_RE},
  {"ruint16_t", MemorySearch::Type::UINT16_RE},
  {"u32",       MemorySearch::Type::UINT32},
  {"uint32",    MemorySearch::Type::UINT32},
  {"uint32_t",  MemorySearch::Type::UINT32},
  {"ru32",      MemorySearch::Type::UINT32_RE},
  {"ruint32",   MemorySearch::Type::UINT32_RE},
  {"ruint32_t", MemorySearch::Type::UINT32_RE},
  {"u64",       MemorySearch::Type::UINT64},
  {"uint64",    MemorySearch::Type::UINT64},
  {"uint64_t",  MemorySearch::Type::UINT64},
  {"ru64",      MemorySearch::Type::UINT64_RE},
  {"ruint64",   MemorySearch::Type::UINT64_RE},
  {"ruint64_t", MemorySearch::Type::UINT64_RE},
  {"s8",        MemorySearch::Type::INT8},
  {"i8",        MemorySearch::Type::INT8},
  {"int8",      MemorySearch::Type::INT8},
  {"int8_t",    MemorySearch::Type::INT8},
  {"s16",       MemorySearch::Type::INT16},
  {"i16",       MemorySearch::Type::INT16},
  {"int16",     MemorySearch::Type::INT16},
  {"int16_t",   MemorySearch::Type::INT16},
  {"rs16",      MemorySearch::Type::INT16_RE},
  {"ri16",      MemorySearch::Type::INT16_RE},
  {"rint16",    MemorySearch::Type::INT16_RE},
  {"rint16_t",  MemorySearch::Type::INT16_RE},
  {"s32",       MemorySearch::Type::INT32},
  {"i32",       MemorySearch::Type::INT32},
  {"int32",     MemorySearch::Type::INT32},
  {"int32_t",   MemorySearch::Type::INT32},
  {"rs32",      MemorySearch::Type::INT32_RE},
  {"ri32",      MemorySearch::Type::INT32_RE},
  {"rint32",    MemorySearch::Type::INT32_RE},
  {"rint32_t",  MemorySearch::Type::INT32_RE},
  {"s64",       MemorySearch::Type::INT64},
  {"i64",       MemorySearch::Type::INT64},
  {"int64",     MemorySearch::Type::INT64},
  {"int64_t",   MemorySearch::Type::INT64},
  {"rs64",      MemorySearch::Type::INT64_RE},
  {"ri64",      MemorySearch::Type::INT64_RE},
  {"rint64",    MemorySearch::Type::INT64_RE},
  {"rint64_t",  MemorySearch::Type::INT64_RE},
  {"f",         MemorySearch::Type::FLOAT},
  {"flt",       MemorySearch::Type::FLOAT},
  {"float",     MemorySearch::Type::FLOAT},
  {"rf",        MemorySearch::Type::FLOAT_RE},
  {"rflt",      MemorySearch::Type::FLOAT_RE},
  {"rfloat",    MemorySearch::Type::FLOAT_RE},
  {"d",         MemorySearch::Type::DOUBLE},
  {"dbl",       MemorySearch::Type::DOUBLE},
  {"double",    MemorySearch::Type::DOUBLE},
  {"rd",        MemorySearch::Type::DOUBLE_RE},
  {"rdbl",      MemorySearch::Type::DOUBLE_RE},
  {"rdouble",   MemorySearch::Type::DOUBLE_RE},
  {"s",         MemorySearch::Type::DATA},
  {"str",       MemorySearch::Type::DATA},
  {"string",    MemorySearch::Type::DATA},
};

MemorySearch::Type MemorySearch::search_type_for_name(const char* name) {
  try {
    return search_type_names.at(name);
  } catch (const out_of_range& e) {
    throw out_of_range(string_printf("no type with name %s", name));
  }
}

const char* MemorySearch::name_for_search_predicate(MemorySearch::Predicate pred) {
  switch (pred) {
    case Predicate::LESS:             return "<";
    case Predicate::GREATER:          return ">";
    case Predicate::LESS_OR_EQUAL:    return "<=";
    case Predicate::GREATER_OR_EQUAL: return ">=";
    case Predicate::EQUAL:            return "==";
    case Predicate::NOT_EQUAL:        return "!=";
    case Predicate::FLAG:             return "$";
    case Predicate::ALL:              return ".";
  }
  return "unknown";
}

static const unordered_map<string, MemorySearch::Predicate> search_predicate_names = {
  {"<",   MemorySearch::Predicate::LESS},
  {">",   MemorySearch::Predicate::GREATER},
  {"<=",  MemorySearch::Predicate::LESS_OR_EQUAL},
  {">=",  MemorySearch::Predicate::GREATER_OR_EQUAL},
  {"=",   MemorySearch::Predicate::EQUAL},
  {"==",  MemorySearch::Predicate::EQUAL},
  {"===", MemorySearch::Predicate::EQUAL}, // lol javascript
  {"!=",  MemorySearch::Predicate::NOT_EQUAL},
  {"<>",  MemorySearch::Predicate::NOT_EQUAL},
  {"$",   MemorySearch::Predicate::FLAG},
  {".",   MemorySearch::Predicate::ALL},
};

MemorySearch::Predicate MemorySearch::search_predicate_for_name(const char* name) {
  try {
    return search_predicate_names.at(name);
  } catch (const out_of_range& e) {
    throw out_of_range(string_printf("no predicate with name %s", name));
  }
}

// returns 1 if the given type is an integral search type
bool MemorySearch::is_integer_search_type(MemorySearch::Type type) {
  return ((int)type >= 0) && ((int)type < (int)Type::FLOAT);
}

// returns 1 if the given type is a reverse-endian search type
bool MemorySearch::is_reverse_endian_search_type(MemorySearch::Type type) {
  return (type == Type::UINT16_RE) || (type == Type::INT16_RE) ||
         (type == Type::UINT32_RE) || (type == Type::INT32_RE) ||
         (type == Type::UINT64_RE) || (type == Type::INT64_RE) ||
         (type == Type::FLOAT_RE) || (type == Type::DOUBLE_RE);
}

size_t MemorySearch::value_size_for_search_type(MemorySearch::Type type) {

  switch (type) {
    case Type::UINT8:
    case Type::INT8:
      return 1;

    case Type::UINT16:
    case Type::UINT16_RE:
    case Type::INT16:
    case Type::INT16_RE:
      return 2;

    case Type::UINT32:
    case Type::UINT32_RE:
    case Type::INT32:
    case Type::INT32_RE:
    case Type::FLOAT:
    case Type::FLOAT_RE:
      return 4;

    case Type::UINT64:
    case Type::UINT64_RE:
    case Type::INT64:
    case Type::INT64_RE:
    case Type::DOUBLE:
    case Type::DOUBLE_RE:
      return 8;

    default:
      return 0;
  }
}
