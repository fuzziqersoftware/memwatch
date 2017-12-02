#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "ProcessMemoryAdapter.hh"

/* possible states for MemorySearch:
 * 
 * known value search: memory and results always valid after 1st search
 * unknown value search: memory always valid after 1st search,
 *                       results invalid until 2nd search
 */

class MemorySearch {
public:
  enum class Type {
    UINT8     = 0,
    UINT16    = 1,
    UINT16_RE = 2,
    UINT32    = 3,
    UINT32_RE = 4,
    UINT64    = 5,
    UINT64_RE = 6,
    INT8      = 7,
    INT16     = 8,
    INT16_RE  = 9,
    INT32     = 10,
    INT32_RE  = 11,
    INT64     = 12,
    INT64_RE  = 13,
    FLOAT     = 14,
    FLOAT_RE  = 15,
    DOUBLE    = 16,
    DOUBLE_RE = 17,
    DATA      = 18,
  };

  enum class Predicate {
    LESS             = 0,
    GREATER          = 1,
    LESS_OR_EQUAL    = 2,
    GREATER_OR_EQUAL = 3,
    EQUAL            = 4,
    NOT_EQUAL        = 5,
    FLAG             = 6,
    ALL              = 7,
  };

  MemorySearch(Type type, bool all_memory = false);
  MemorySearch(const MemorySearch& other) = default;
  ~MemorySearch() = default;

  Type get_type() const;
  bool has_memory() const;
  bool has_valid_results() const;
  bool is_all_memory() const;
  const std::vector<uint64_t>& get_results() const;

  void check_can_update(Predicate predicate, const std::string& data_c);
  void update(std::shared_ptr<std::vector<ProcessMemoryAdapter::Region>> new_memory,
      Predicate predicate, const std::string& data, size_t max_results,
      FILE* progress_file = NULL);
  void delete_results(uint64_t start, uint64_t end);

  static const char* name_for_search_type(Type type);
  static const char* short_name_for_search_type(Type type);
  static Type search_type_for_name(const char* name);
  static const char* name_for_search_predicate(Predicate pred);
  static Predicate search_predicate_for_name(const char* name);

  static bool is_integer_search_type(Type type);
  static bool is_reverse_endian_search_type(Type type);
  static size_t value_size_for_search_type(Type type);

private:
  Type type;
  bool results_valid;
  bool all_memory;
  std::shared_ptr<std::vector<ProcessMemoryAdapter::Region>> memory;
  uint64_t prev_size;
  std::vector<uint64_t> results;
};
