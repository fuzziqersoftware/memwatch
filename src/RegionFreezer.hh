#pragma once

#include <mach/mach_vm.h>
#include <sys/types.h>

#include <map>
#include <shared_mutex>
#include <string>
#include <thread>

#include "ProcessMemoryAdapter.hh"


class RegionFreezer {
public:
  RegionFreezer() = delete;
  RegionFreezer(std::shared_ptr<ProcessMemoryAdapter> adapter);
  ~RegionFreezer();

  void freeze(const std::string& name, mach_vm_address_t addr,
      const std::string& data, bool enable = true);
  void freeze_array(const std::string& name, mach_vm_address_t base_addr,
      size_t max_items, const std::string& data, const std::string& data_mask,
      const std::string* null_data = NULL,
      const std::string* null_data_mask = NULL, bool enable = true);

  size_t unfreeze_name(const std::string& name);
  size_t unfreeze_addr(mach_vm_address_t addr);
  bool unfreeze_index(size_t index);
  size_t unfreeze_all();

  size_t enable_name(const std::string& name, bool enable);
  size_t enable_addr(mach_vm_address_t addr, bool enable);
  bool enable_index(size_t index, bool enable);
  size_t enable_all(bool enable);

  size_t frozen_count() const;

  void print_regions(FILE* stream, bool with_data = false) const;
  void print_regions_commands(FILE* stream) const;

private:
  class Region {
  public:
    std::string name;
    uint64_t index;

    mach_vm_address_t addr;
    std::string data;
    std::string error;
    bool enable;

    Region(const std::string& name, uint64_t index, mach_vm_address_t addr,
        const std::string& data, bool enable = true);
    virtual ~Region() = default;

    virtual void print(FILE* stream, bool with_data = false) const;
    virtual void print_command(FILE* stream) const;

    virtual void write(std::shared_ptr<ProcessMemoryAdapter> adapter);
  };
  class ArrayRegion : public Region {
  public:
    size_t num_items;
    std::string data_mask;
    std::string null_data;
    std::string null_data_mask;

    ArrayRegion(const std::string& name, uint64_t index, mach_vm_address_t addr,
        size_t num_items, const std::string& data, const std::string& data_mask,
        const std::string* null_data, const std::string* null_data_mask,
        bool enable = true);
    virtual ~ArrayRegion() = default;

    virtual void print(FILE* stream, bool with_data = false) const;
    virtual void print_command(FILE* stream) const;

    virtual void write(std::shared_ptr<ProcessMemoryAdapter> adapter);
  };

  std::shared_ptr<ProcessMemoryAdapter> adapter;

  std::unordered_multimap<std::string, std::shared_ptr<Region>> regions_by_name;
  std::unordered_multimap<mach_vm_address_t, std::shared_ptr<Region>> regions_by_addr;
  std::map<size_t, std::shared_ptr<Region>> regions_by_index;
  std::atomic<size_t> next_index;

  mutable std::shared_mutex lock;
  std::atomic<bool> should_exit;
  std::thread thread;

  void add_region(std::shared_ptr<Region> rgn);

  void run_write_thread();
};
