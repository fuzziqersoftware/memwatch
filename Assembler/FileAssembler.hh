#include <stddef.h>

#include <string>
#include <vector>
#include <unordered_set>
#include <map>



struct AssembledFile {
  std::string code;
  std::unordered_set<size_t> patch_offsets;
  std::multimap<size_t, std::string> label_offsets;
  std::vector<std::string> errors;
};

AssembledFile assemble_file(const std::string& data);
