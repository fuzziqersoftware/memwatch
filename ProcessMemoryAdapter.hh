#pragma once

#include <mach/mach_vm.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>



class ProcessMemoryAdapter {
public:
  struct Region {
    enum Protection {
      READABLE = 1,
      WRITABLE = 2,
      EXECUTABLE = 4,
      ALL_ACCESS = 7,
    };

    mach_vm_address_t addr;
    mach_vm_size_t size;
    uint8_t protection;
    uint8_t max_protection;
    int inherit;
    int shared;
    int reserved;
    vm_behavior_t behavior;
    uint16_t wired_count;

    std::string data;

    Region(mach_vm_address_t addr,
        mach_vm_size_t size, uint8_t protection, uint8_t max_protection,
        int inherit, int shared, int reserved, vm_behavior_t behavior,
        uint16_t wired_count);
    ~Region() = default;

    mach_vm_address_t end_addr() const;
  };

  struct ThreadState {
    int is64;
    mach_msg_type_number_t count, fcount, dcount;
    union {
      x86_thread_state32_t st32;
      x86_thread_state64_t st64;
    };
    union {
      x86_float_state32_t fl32;
      x86_float_state64_t fl64;
    };
    union {
      x86_debug_state32_t db32;
      x86_debug_state64_t db64;
    };

    void set_register_by_name(const std::string& name, uint64_t value);
  };

  ProcessMemoryAdapter(pid_t pid);
  ~ProcessMemoryAdapter() = default;

  void attach(pid_t pid);

  Region get_region(mach_vm_address_t addr, bool read_data = false);
  std::vector<Region> get_all_regions(bool read_data = false);
  std::vector<Region> get_target_regions(
      const std::vector<uint64_t>& target_addresses, bool read_data = false);

  mach_vm_address_t allocate(vm_address_t addr, mach_vm_size_t size);
  void deallocate(vm_address_t addr, mach_vm_size_t size);

  void set_protection(mach_vm_address_t addr, mach_vm_size_t size, uint8_t prot,
      uint8_t mask);

  std::string read(mach_vm_address_t addr, size_t size);
  void read(Region& region);
  void read(std::vector<Region>& regions);
  void write(mach_vm_address_t addr, const std::string& data);
  void write(mach_vm_address_t addr, const void* data, size_t size);

  void pause();
  void resume();
  void terminate();

  ThreadState get_thread_registers(mach_port_t thread_port);
  void set_thread_registers(mach_port_t thread_port, const ThreadState& st);
  std::unordered_map<mach_port_t, ThreadState> get_threads_registers();
  void set_threads_registers(const std::unordered_map<mach_port_t, ThreadState>& ts);

  mach_port_t create_thread(mach_vm_address_t ip, mach_vm_address_t sp);
  void terminate_thread(mach_port_t thread);

private:
  std::pair<std::unique_ptr<thread_act_t, std::function<void(thread_act_port_array_t x)>>, mach_msg_type_number_t>
    get_thread_ports();

  pid_t pid;
  vm_map_t task;
};

class PauseGuard {
public:
  PauseGuard(std::shared_ptr<ProcessMemoryAdapter> adapter);
  ~PauseGuard();
private:
  std::shared_ptr<ProcessMemoryAdapter> adapter;
};
