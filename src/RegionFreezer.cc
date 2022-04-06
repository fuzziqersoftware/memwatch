#define _STDC_FORMAT_MACROS

#include "RegionFreezer.hh"

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <phosg/Strings.hh>

using namespace std;



static int memcmp_mask(const void* a, const void* b, const void* mask, size_t size) {

  if (!mask) {
    return memcmp(a, b, size);
  }

  int masked_differences = 0;
  uint8_t *va = (uint8_t*)a, *vb = (uint8_t*)b, *vmask = (uint8_t*)mask;
  for (; size > 0; size--, va++, vb++, vmask++) {
    if (va[0] != vb[0]) {
      if (!vmask[0])
        masked_differences = 1;
      else
        return -1;
    }
  }
  return masked_differences;
}



RegionFreezer::RegionFreezer(shared_ptr<ProcessMemoryAdapter> adapter) :
    adapter(adapter), regions_by_name(), regions_by_addr(), regions_by_index(),
    next_index(0), lock(), should_exit(false),
    thread(&RegionFreezer::run_write_thread, this) { }

RegionFreezer::~RegionFreezer() {
  this->should_exit = true;
  this->thread.join();
}

void RegionFreezer::freeze(const string& name, mach_vm_address_t addr,
    const string& data, bool enable) {
  shared_ptr<Region> rgn(new Region(name, this->next_index++, addr, data,
      enable));
  this->add_region(rgn);
}

void RegionFreezer::freeze_array(const string& name,
    mach_vm_address_t base_addr, size_t max_items, const string& data,
    const string& data_mask, const string* null_data,
    const string* null_data_mask, bool enable) {
  shared_ptr<Region> rgn(new ArrayRegion(name, this->next_index++, base_addr,
      max_items, data, data_mask, null_data, null_data_mask, enable));
  this->add_region(rgn);
}

template <typename K, typename V>
static void erase_by_value(unordered_multimap<K, V>& m, K& k, V& v) {
  auto its = m.equal_range(k);
  for (auto it = its.first; it != its.second;) {
    if (it->second == v) {
      it = m.erase(it);
    } else {
      it++;
    }
  }
}

size_t RegionFreezer::unfreeze_name(const string& name) {
  size_t num_deleted = 0;

  unique_lock g(this->lock);
  auto its = this->regions_by_name.equal_range(name);
  for (auto it = its.first; it != its.second; it++) {
    erase_by_value(this->regions_by_addr, it->second->addr, it->second);
    this->regions_by_index.erase(it->second->index);
    num_deleted++;
  }
  this->regions_by_name.erase(its.first, its.second);

  return num_deleted;
}

size_t RegionFreezer::unfreeze_addr(mach_vm_address_t addr) {
  size_t num_deleted = 0;

  unique_lock g(this->lock);
  auto its = this->regions_by_addr.equal_range(addr);
  for (auto it = its.first; it != its.second; it++) {
    erase_by_value(this->regions_by_name, it->second->name, it->second);
    this->regions_by_index.erase(it->second->index);
    num_deleted++;
  }
  this->regions_by_addr.erase(its.first, its.second);

  return num_deleted;
}

bool RegionFreezer::unfreeze_index(size_t index) {
  unique_lock g(this->lock);
  auto it = this->regions_by_index.find(index);
  if (it == this->regions_by_index.end()) {
    return false;
  }
  erase_by_value(this->regions_by_name, it->second->name, it->second);
  erase_by_value(this->regions_by_addr, it->second->addr, it->second);
  this->regions_by_index.erase(it);

  return true;
}

size_t RegionFreezer::unfreeze_all() {
  unique_lock g(this->lock);
  size_t num_regions = this->regions_by_index.size();
  this->regions_by_name.clear();
  this->regions_by_name.clear();
  this->regions_by_index.clear();
  return num_regions;
}

size_t RegionFreezer::enable_name(const string& name, bool enable) {
  size_t num_changed = 0;

  unique_lock g(this->lock);
  auto its = this->regions_by_name.equal_range(name);
  for (auto it = its.first; it != its.second; it++) {
    if (it->second->enable == enable) {
      continue;
    }
    it->second->enable = enable;
    num_changed++;
  }

  return num_changed;
}

size_t RegionFreezer::enable_addr(mach_vm_address_t addr, bool enable) {
  size_t num_changed = 0;

  unique_lock g(this->lock);
  auto its = this->regions_by_addr.equal_range(addr);
  for (auto it = its.first; it != its.second; it++) {
    if (it->second->enable == enable) {
      continue;
    }
    it->second->enable = enable;
    num_changed++;
  }

  return num_changed;
}

bool RegionFreezer::enable_index(size_t index, bool enable) {
  try {
    unique_lock g(this->lock);
    auto rgn = this->regions_by_index.at(index);
    if (rgn->enable == enable) {
      return false;
    }
    rgn->enable = enable;
    return true;
  } catch (const out_of_range& e) {
    return false;
  }
}

size_t RegionFreezer::enable_all(bool enable) {
  size_t num_changed = 0;

  unique_lock g(this->lock);
  for (auto& it : this->regions_by_index) {
    if (it.second->enable == enable) {
      continue;
    }
    it.second->enable = enable;
    num_changed++;
  }

  return num_changed;
}

size_t RegionFreezer::frozen_count() const {
  return this->regions_by_index.size();
}

void RegionFreezer::print_regions(FILE* stream, bool with_data) const {
  shared_lock g(this->lock);

  if (this->regions_by_index.empty()) {
    fprintf(stream, "no regions frozen\n");
    return;
  }

  fprintf(stream, "frozen regions:\n");
  for (auto& it : this->regions_by_index) {
    it.second->print(stream, with_data);
  }
}

void RegionFreezer::print_regions_commands(FILE* stream) const {
  shared_lock g(this->lock);

  if (this->regions_by_index.empty()) {
    fprintf(stream, "no regions frozen\n");
    return;
  }

  fprintf(stream, "frozen regions:\n");
  for (auto& it : this->regions_by_index) {
    it.second->print_command(stream);
  }
}



RegionFreezer::Region::Region(const std::string& name, uint64_t index,
    mach_vm_address_t addr, const std::string& data, bool enable) : name(name),
    index(index), addr(addr), data(data), error(), enable(enable) { }

void RegionFreezer::Region::print(FILE* stream, bool with_data) const {
  fprintf(stream, "%4" PRIu64 ": %016" PRIX64 ":%016zX %s%s",
      this->index, this->addr, this->data.size(), this->name.c_str(),
      this->enable ? "" : " (disabled)");
  if (!this->error.empty()) {
    fprintf(stream, " (error: %s)\n", this->error.c_str());
  } else {
    fputc('\n', stream);
  }
  if (with_data) {
    fprintf(stream, "data:\n");
    print_data(stream, this->data.data(), this->data.size(), this->addr);
  }
}

void RegionFreezer::Region::print_command(FILE* stream) const {
  string data = format_data_string(this->data);
  fprintf(stream, "f +n%s %016" PRIX64 " %s%s\n", this->name.c_str(), this->addr,
      data.c_str(), this->enable ? "" : " +d");
}

void RegionFreezer::Region::write(shared_ptr<ProcessMemoryAdapter> adapter) {
  try {
    adapter->write(this->addr, this->data);
    this->error.clear();
  } catch (const exception& e) {
    this->error = e.what();
  }
}



RegionFreezer::ArrayRegion::ArrayRegion(const std::string& name, uint64_t index,
    mach_vm_address_t addr, size_t num_items, const std::string& data,
    const std::string& data_mask, const std::string* null_data,
    const std::string* null_data_mask, bool enable) :
    Region(name, index, addr, data, enable), num_items(num_items),
    data_mask(data_mask), null_data(), null_data_mask() {
  if (null_data && null_data_mask) {
    this->null_data = *null_data;
    this->null_data_mask = *null_data_mask;
  }
}

void RegionFreezer::ArrayRegion::print(FILE* stream, bool with_data) const {
  fprintf(stream, "%4" PRIu64 ": %016" PRIX64 ":%016zX [array:%zu] %s%s",
      this->index, this->addr, this->data.size(), this->num_items,
      this->name.c_str(), this->enable ? "" : " (disabled)");
  if (!this->error.empty()) {
    fprintf(stream, " (error: %s)\n", this->error.c_str());
  } else {
    fputc('\n', stream);
  }
  if (with_data) {
    fprintf(stream, "data:\n");
    print_data(stream, this->data.data(), this->data.size());
    fprintf(stream, "data mask:\n");
    print_data(stream, this->data_mask.data(), this->data_mask.size());
    if (!this->null_data.empty() && !this->null_data_mask.empty()) {
      fprintf(stream, "null data:\n");
      print_data(stream, this->null_data.data(), this->null_data.size());
      fprintf(stream, "null data mask:\n");
      print_data(stream, this->null_data_mask.data(), this->null_data_mask.size());
    }
  }
}

void RegionFreezer::ArrayRegion::print_command(FILE* stream) const {
  string data = format_data_string(this->data, &this->data_mask);
  fprintf(stream, "f +n%s %016" PRIX64 " +m%zu %s%s", this->name.c_str(),
      this->addr, this->num_items, data.c_str(), this->enable ? "" : " +d");

  if (!this->null_data.empty() && !this->null_data_mask.empty()) {
    string null_data = format_data_string(this->null_data, &this->null_data_mask);
    fprintf(stream, " +N%s", null_data.c_str());
  }
  fputc('\n', stream);
}

void RegionFreezer::ArrayRegion::write(shared_ptr<ProcessMemoryAdapter> adapter) {
  try {
    string contents = adapter->read(this->addr, this->num_items * this->data.size());

    // find the first null entry in the array, OR find an entry that matches
    // the frozen data
    size_t x;
    for (x = 0; x < this->num_items; x++) {
      const char* item = contents.c_str() + (x * this->data.size());

      int cmp_result = memcmp_mask(item, this->data.data(),
          this->data_mask.data(), this->data.size());
      if (cmp_result == 0) { // data already present in array
        this->error.clear();
        return;
      } else if (cmp_result == 1) { // data present with masked differences
        adapter->write(this->addr + (x * this->data.size()), this->data);
        return;
      }

      // if null data is given, check if this entry is null (and mask if needed)
      if (!this->null_data.empty()) {
        cmp_result = memcmp_mask(item, this->null_data.data(),
            this->null_data_mask.data(), this->data.size());
        if (cmp_result >= 0) {
          break; // entry is null (possibly only by mask but we don't care)
        }
      } else {
        size_t y;
        for (y = 0; y < this->data.size(); y++) {
          if (item[y] != 0) {
            break;
          }
        }
        if (y == this->data.size()) {
          break;
        }
      }
    }

    // no null entries? then don't write anything
    if (x == this->num_items) {
      this->error = "no available spaces";
    } else {
      adapter->write(this->addr + (x * this->data.size()), this->data);
    }

  } catch (const exception& e) {
    this->error = e.what();
  }
}

void RegionFreezer::add_region(shared_ptr<Region> rgn) {
  unique_lock g(this->lock);
  this->regions_by_name.emplace(rgn->name, rgn);
  this->regions_by_addr.emplace(rgn->addr, rgn);
  this->regions_by_index.emplace(rgn->index, rgn);
}

void RegionFreezer::run_write_thread() {
  while (!this->should_exit) {
    {
      unique_lock g(this->lock);
      for (auto& it : this->regions_by_index) {
        if (it.second->enable) {
          it.second->write(this->adapter);
        }
      }
    }
    usleep(10000);
  }
}
