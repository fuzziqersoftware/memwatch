#define _STDC_FORMAT_MACROS

#include "ProcessMemoryAdapter.hh"

#include <inttypes.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include <phosg/Strings.hh>

using namespace std;


ProcessMemoryAdapter::Region::Region(mach_vm_address_t addr,
    size_t size, uint8_t protection, uint8_t max_protection,
    int inherit, int shared, int reserved, vm_behavior_t behavior,
    uint16_t wired_count) : addr(addr), size(size), protection(protection),
    max_protection(max_protection), inherit(inherit), shared(shared),
    reserved(reserved), behavior(behavior), wired_count(wired_count), data() { }

mach_vm_address_t ProcessMemoryAdapter::Region::end_addr() const {
  return this->addr + this->size;
}



ProcessMemoryAdapter::ProcessMemoryAdapter(pid_t pid) {
  this->attach(pid);
}

void ProcessMemoryAdapter::attach(pid_t pid) {
  this->pid = pid;

  kern_return_t ret = task_for_pid(current_task(), this->pid, &this->task);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t get task for pid %d (%d)", this->pid, ret));
  }
}


ProcessMemoryAdapter::Region ProcessMemoryAdapter::get_region(
    mach_vm_address_t addr, bool read_data) {
  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_size = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;
  memset(&info, 0, sizeof(info));

  kern_return_t ret = mach_vm_region(this->task, &addr, &size,
      VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &info_size, &object_name);
  if (ret != KERN_SUCCESS) {
    throw out_of_range("no region found");
  }

  uint8_t prot =
      ((info.protection & VM_PROT_READ) ? Region::Protection::READABLE : 0) |
      ((info.protection & VM_PROT_WRITE) ? Region::Protection::WRITABLE : 0) |
      ((info.protection & VM_PROT_EXECUTE) ? Region::Protection::EXECUTABLE : 0);
  uint8_t max_prot =
      ((info.max_protection & VM_PROT_READ) ? Region::Protection::READABLE : 0) |
      ((info.max_protection & VM_PROT_WRITE) ? Region::Protection::WRITABLE : 0) |
      ((info.max_protection & VM_PROT_EXECUTE) ? Region::Protection::EXECUTABLE : 0);
  Region r(addr, size, prot, max_prot, info.inheritance,
        info.shared, info.reserved, info.behavior, info.user_wired_count);

  if (read_data) {
    try {
      this->read(r);
    } catch (const runtime_error& e) { }
  }

  return r;
}

vector<ProcessMemoryAdapter::Region> ProcessMemoryAdapter::get_all_regions(
    bool read_data) {

  vector<Region> ret;

  mach_vm_address_t addr = 0;
  for (;;) {
    try {
      ret.emplace_back(this->get_region(addr, read_data));
      addr = ret.back().end_addr();
    } catch (const out_of_range& e) {
      break;
    }
  }

  return ret;
}

vector<ProcessMemoryAdapter::Region> ProcessMemoryAdapter::get_target_regions(
    const std::vector<uint64_t>& target_addresses, bool read_data) {

  auto target_it = target_addresses.begin();
  vector<Region> ret;

  mach_vm_address_t addr = 0;
  for (;;) {
    if (target_it == target_addresses.end()) {
      break;
    }

    try {
      Region r = this->get_region(addr, false);
      addr = r.end_addr();

      // advance target_it until it's not before this region
      for (; target_it != target_addresses.end() && *target_it < r.addr; target_it++);

      // if there's a target address in this region, read the data and add it to
      // the list
      if (*target_it < r.end_addr()) {
        if (read_data) {
          try {
            this->read(r);
          } catch (const runtime_error& e) { }
        }
        ret.emplace_back(move(r));
      }
    } catch (const out_of_range& e) {
      break;
    }
  }

  return ret;
}

mach_vm_address_t ProcessMemoryAdapter::allocate(vm_address_t addr,
    size_t size) {
  kern_return_t ret = vm_allocate(this->task, &addr, size, (addr == 0));
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t allocate memory in process %d at %016lX:%016zX (%d)", this->pid, addr, size, ret));
  }
  return addr;
}

void ProcessMemoryAdapter::deallocate(vm_address_t addr, size_t size) {
  kern_return_t ret = vm_deallocate(this->task, addr, size);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t deallocate memory in process %d at %016lX:%016zX (%d)", this->pid, addr, size, ret));
  }
}

void ProcessMemoryAdapter::set_protection(mach_vm_address_t addr, size_t size,
    uint8_t prot, uint8_t prot_mask) {
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get this region
  mach_vm_size_t vm_size = size;
  kern_return_t ret = mach_vm_region(this->task, &addr, &vm_size,
      VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &info_count, &object_name);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read region in process %d at %016" PRIX64 ":%016zX (%d)", this->pid, addr, size, ret));
  }

  // get the attributes and return them
  if (prot_mask & Region::Protection::READABLE) {
    info.protection &= ~VM_PROT_READ;
    if (prot & Region::Protection::READABLE) {
      info.protection |= VM_PROT_READ;
    }
  }
  if (prot_mask & Region::Protection::WRITABLE) {
    info.protection &= ~VM_PROT_WRITE;
    if (prot & Region::Protection::WRITABLE) {
      info.protection |= VM_PROT_WRITE;
    }
  }
  if (prot_mask & Region::Protection::EXECUTABLE) {
    info.protection &= ~VM_PROT_EXECUTE;
    if (prot & Region::Protection::EXECUTABLE) {
      info.protection |= VM_PROT_EXECUTE;
    }
  }

  ret = mach_vm_protect(this->task, addr, vm_size, 0ULL, info.protection);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t set protection in process %d at %016" PRIX64 ":%016" PRIX64 " (%d)", this->pid, addr, vm_size, ret));
  }
}

string ProcessMemoryAdapter::read(mach_vm_address_t addr, size_t size) {
  mach_vm_size_t result_size = size;
  string data(size, 0);
  vm_offset_t data_ptr = (vm_offset_t)const_cast<char*>(data.data());
  kern_return_t ret = mach_vm_read_overwrite(this->task, addr, size,
      data_ptr, &result_size);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read from process %d at %016" PRIX64 ":%016zX (%d)", this->pid, addr, size, ret));
  }
  data.resize(result_size);
  return data;
}

void ProcessMemoryAdapter::read(Region& r) {
  // make the region readable if we have to
  if (!(r.protection & Region::Protection::READABLE)) {
    this->set_protection(r.addr, r.size, r.protection | Region::Protection::READABLE,
        Region::Protection::ALL_ACCESS);
  }

  r.data = this->read(r.addr, r.size);

  // if we made the region readable, restore its previous protection
  if (!(r.protection & Region::Protection::READABLE)) {
    this->set_protection(r.addr, r.size, r.protection, Region::Protection::ALL_ACCESS);
  }
}

void ProcessMemoryAdapter::read(std::vector<Region>& regions) {
  for (auto& r : regions) {
    this->read(r);
  }
}

void ProcessMemoryAdapter::write(mach_vm_address_t addr, const string& data) {
  this->write(addr, data.data(), data.size());
}

void ProcessMemoryAdapter::write(mach_vm_address_t addr, const void* data,
    size_t size) {
  kern_return_t ret = mach_vm_write(this->task, addr, (vm_offset_t)data, size);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t write to process %d at %016" PRIX64 ":%016zX (%d)", this->pid, addr, size, ret));
  }
}

void ProcessMemoryAdapter::pause() {
  kern_return_t ret = task_suspend(this->task);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t pause process %d (%d)", this->pid, ret));
  }
}

void ProcessMemoryAdapter::resume() {
  kern_return_t ret = task_resume(this->task);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t resume process %d (%d)", this->pid, ret));
  }
}

void ProcessMemoryAdapter::terminate() {
  kern_return_t ret = task_terminate(this->task);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t terminate process %d (%d)", this->pid, ret));
  }
}



pair<unique_ptr<thread_act_t, function<void(thread_act_port_array_t x)>>, mach_msg_type_number_t>
ProcessMemoryAdapter::get_thread_ports() {

  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  kern_return_t ret = task_threads(this->task, &thread_list, &thread_count);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read thread list for process %d", this->pid));
  }

  auto deleter = [thread_count] (thread_act_port_array_t x) {
    vm_deallocate(mach_task_self(), (vm_address_t)x, (thread_count * sizeof(int)));
  };
  unique_ptr<thread_act_t, function<void(thread_act_port_array_t x)>> ptr(thread_list, deleter);

  return make_pair(move(ptr), thread_count);
}

void ProcessMemoryAdapter::terminate_thread(mach_port_t thread_port) {
  kern_return_t ret = thread_terminate(thread_port);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t terminate thread (%d)", ret));
  }
}

PauseGuard::PauseGuard(shared_ptr<ProcessMemoryAdapter> adapter) :
    adapter(adapter) {
  if (adapter) {
    adapter->pause();
  }
}

PauseGuard::~PauseGuard() {
  if (adapter) {
    adapter->resume();
  }
}
