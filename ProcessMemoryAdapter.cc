#include "ProcessMemoryAdapter.hh"

#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include <phosg/Strings.hh>

using namespace std;


ProcessMemoryAdapter::Region::Region(mach_vm_address_t addr,
    mach_vm_size_t size, uint8_t protection, uint8_t max_protection,
    int inherit, int shared, int reserved, vm_behavior_t behavior,
    uint16_t wired_count, shared_ptr<string> data) : addr(addr), size(size),
    protection(protection), max_protection(max_protection), inherit(inherit),
    shared(shared), reserved(reserved), behavior(behavior),
    wired_count(wired_count), data(data) { }

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

vector<ProcessMemoryAdapter::Region> ProcessMemoryAdapter::get_regions(
    bool read_data) {

  vector<Region> ret;

  mach_vm_address_t addr = 0;
  for (;;) {
    try {
      ret.emplace_back(this->get_region(addr, read_data));
      addr = ret.back().addr + ret.back().size;
    } catch (const out_of_range& e) {
      break;
    }
  }

  return ret;
}

mach_vm_address_t ProcessMemoryAdapter::allocate(vm_address_t addr,
    mach_vm_size_t size) {
  kern_return_t ret = vm_allocate(this->task, &addr, size, (addr == 0));
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t allocate memory in process %d at %016llX:%016llX (%d)", this->pid, addr, size, ret));
  }
  return addr;
}

void ProcessMemoryAdapter::deallocate(vm_address_t addr, mach_vm_size_t size) {
  kern_return_t ret = vm_deallocate(this->task, addr, size);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t deallocate memory in process %d at %016llX:%016llX (%d)", this->pid, addr, size, ret));
  }
}

void ProcessMemoryAdapter::set_protection(mach_vm_address_t addr,
    mach_vm_size_t size, uint8_t prot, uint8_t prot_mask) {
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get this region
  kern_return_t ret = mach_vm_region(this->task, &addr, &size,
      VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &info_count, &object_name);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read region in process %d at %016llX:%016llX (%d)", this->pid, addr, size, ret));
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

  ret = mach_vm_protect(this->task, addr, size, 0ULL, info.protection);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t set protection in process %d at %016llX:%016llX (%d)", this->pid, addr, size, ret));
  }
}

string ProcessMemoryAdapter::read(mach_vm_address_t addr, size_t size) {

  mach_vm_size_t result_size = size;
  string data(size, 0);
  vm_offset_t data_ptr = (vm_offset_t)const_cast<char*>(data.data());
  kern_return_t ret = mach_vm_read_overwrite(this->task, addr, size,
      data_ptr, &result_size);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read from process %d at %016llX:%016llX (%d)", this->pid, addr, size, ret));
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

  r.data.reset(new string(this->read(r.addr, r.size)));

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
  kern_return_t ret = mach_vm_write(this->task, addr, (vm_offset_t)data.data(), data.size());
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t write to process %d at %016llX:%016llX (%d)", this->pid, addr, data.size(), ret));
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



////////////////////////////////////////////////////////////////////////////////
// register modification facilities

void ProcessMemoryAdapter::ThreadState::set_register_by_name(const string& name,
    uint64_t value) {

  struct offsets_t {
    int offset32;
    int offset64;
    int size32;
    int size64;
  };

  static const unordered_map<string, offsets_t> offsets = {

    // 64-bit regs
    {"rax", {0, offsetof(ThreadState, st64.__rax), 0, 8}},
    {"rcx", {0, offsetof(ThreadState, st64.__rcx), 0, 8}},
    {"rdx", {0, offsetof(ThreadState, st64.__rdx), 0, 8}},
    {"rbx", {0, offsetof(ThreadState, st64.__rbx), 0, 8}},
    {"rsp", {0, offsetof(ThreadState, st64.__rsp), 0, 8}},
    {"rbp", {0, offsetof(ThreadState, st64.__rbp), 0, 8}},
    {"rsi", {0, offsetof(ThreadState, st64.__rsi), 0, 8}},
    {"rdi", {0, offsetof(ThreadState, st64.__rdi), 0, 8}},
    {"r8",  {0, offsetof(ThreadState, st64.__r8), 0, 8}},
    {"r9",  {0, offsetof(ThreadState, st64.__r9), 0, 8}},
    {"r10", {0, offsetof(ThreadState, st64.__r10), 0, 8}},
    {"r11", {0, offsetof(ThreadState, st64.__r11), 0, 8}},
    {"r12", {0, offsetof(ThreadState, st64.__r12), 0, 8}},
    {"r13", {0, offsetof(ThreadState, st64.__r13), 0, 8}},
    {"r14", {0, offsetof(ThreadState, st64.__r14), 0, 8}},
    {"r15", {0, offsetof(ThreadState, st64.__r15), 0, 8}},
    {"rflags", {0, offsetof(ThreadState, st64.__rflags), 0, 8}},

    // 32-bit regs
    {"eax", {offsetof(ThreadState, st32.__eax), offsetof(ThreadState, st64.__rax), 4, 4}},
    {"ecx", {offsetof(ThreadState, st32.__ecx), offsetof(ThreadState, st64.__rcx), 4, 4}},
    {"edx", {offsetof(ThreadState, st32.__edx), offsetof(ThreadState, st64.__rdx), 4, 4}},
    {"ebx", {offsetof(ThreadState, st32.__ebx), offsetof(ThreadState, st64.__rbx), 4, 4}},
    {"esp", {offsetof(ThreadState, st32.__esp), offsetof(ThreadState, st64.__rsp), 4, 4}},
    {"ebp", {offsetof(ThreadState, st32.__ebp), offsetof(ThreadState, st64.__rbp), 4, 4}},
    {"esi", {offsetof(ThreadState, st32.__esi), offsetof(ThreadState, st64.__rsi), 4, 4}},
    {"edi", {offsetof(ThreadState, st32.__edi), offsetof(ThreadState, st64.__rdi), 4, 4}},
    {"r8d",  {0, offsetof(ThreadState, st64.__r8),  0, 4}},
    {"r9d",  {0, offsetof(ThreadState, st64.__r9),  0, 4}},
    {"r10d", {0, offsetof(ThreadState, st64.__r10), 0, 4}},
    {"r11d", {0, offsetof(ThreadState, st64.__r11), 0, 4}},
    {"r12d", {0, offsetof(ThreadState, st64.__r12), 0, 4}},
    {"r13d", {0, offsetof(ThreadState, st64.__r13), 0, 4}},
    {"r14d", {0, offsetof(ThreadState, st64.__r14), 0, 4}},
    {"r15d", {0, offsetof(ThreadState, st64.__r15), 0, 4}},
    {"eflags", {offsetof(ThreadState, st32.__eflags), offsetof(ThreadState, st64.__rflags), 4, 4}},

    // 16-bit regs
    {"ax", {offsetof(ThreadState, st32.__eax), offsetof(ThreadState, st64.__rax), 2, 2}},
    {"cx", {offsetof(ThreadState, st32.__ecx), offsetof(ThreadState, st64.__rcx), 2, 2}},
    {"dx", {offsetof(ThreadState, st32.__edx), offsetof(ThreadState, st64.__rdx), 2, 2}},
    {"bx", {offsetof(ThreadState, st32.__ebx), offsetof(ThreadState, st64.__rbx), 2, 2}},
    {"sp", {offsetof(ThreadState, st32.__esp), offsetof(ThreadState, st64.__rsp), 2, 2}},
    {"bp", {offsetof(ThreadState, st32.__ebp), offsetof(ThreadState, st64.__rbp), 2, 2}},
    {"si", {offsetof(ThreadState, st32.__esi), offsetof(ThreadState, st64.__rsi), 2, 2}},
    {"di", {offsetof(ThreadState, st32.__edi), offsetof(ThreadState, st64.__rdi), 2, 2}},
    {"r8w",  {0, offsetof(ThreadState, st64.__r8),  0, 2}},
    {"r9w",  {0, offsetof(ThreadState, st64.__r9),  0, 2}},
    {"r10w", {0, offsetof(ThreadState, st64.__r10), 0, 2}},
    {"r11w", {0, offsetof(ThreadState, st64.__r11), 0, 2}},
    {"r12w", {0, offsetof(ThreadState, st64.__r12), 0, 2}},
    {"r13w", {0, offsetof(ThreadState, st64.__r13), 0, 2}},
    {"r14w", {0, offsetof(ThreadState, st64.__r14), 0, 2}},
    {"r15w", {0, offsetof(ThreadState, st64.__r15), 0, 2}},
    {"flags", {offsetof(ThreadState, st32.__eflags), offsetof(ThreadState, st64.__rflags), 2, 2}},

    // 8-bit regs
    {"al", {offsetof(ThreadState, st32.__eax), offsetof(ThreadState, st64.__rax), 1, 1}},
    {"cl", {offsetof(ThreadState, st32.__ecx), offsetof(ThreadState, st64.__rcx), 1, 1}},
    {"dl", {offsetof(ThreadState, st32.__edx), offsetof(ThreadState, st64.__rdx), 1, 1}},
    {"bl", {offsetof(ThreadState, st32.__ebx), offsetof(ThreadState, st64.__rbx), 1, 1}},
    {"ah", {offsetof(ThreadState, st32.__eax) + 1, offsetof(ThreadState, st64.__rax) + 1, 1, 1}},
    {"ch", {offsetof(ThreadState, st32.__ecx) + 1, offsetof(ThreadState, st64.__rcx) + 1, 1, 1}},
    {"dh", {offsetof(ThreadState, st32.__edx) + 1, offsetof(ThreadState, st64.__rdx) + 1, 1, 1}},
    {"bh", {offsetof(ThreadState, st32.__ebx) + 1, offsetof(ThreadState, st64.__rbx) + 1, 1, 1}},
    {"spl", {offsetof(ThreadState, st32.__esp), offsetof(ThreadState, st64.__rsp), 1, 1}},
    {"bpl", {offsetof(ThreadState, st32.__ebp), offsetof(ThreadState, st64.__rbp), 1, 1}},
    {"sil", {offsetof(ThreadState, st32.__esi), offsetof(ThreadState, st64.__rsi), 1, 1}},
    {"dil", {offsetof(ThreadState, st32.__edi), offsetof(ThreadState, st64.__rdi), 1, 1}},
    {"r8b",  {0, offsetof(ThreadState, st64.__r8),  0, 1}},
    {"r9b",  {0, offsetof(ThreadState, st64.__r9),  0, 1}},
    {"r10b", {0, offsetof(ThreadState, st64.__r10), 0, 1}},
    {"r11b", {0, offsetof(ThreadState, st64.__r11), 0, 1}},
    {"r12b", {0, offsetof(ThreadState, st64.__r12), 0, 1}},
    {"r13b", {0, offsetof(ThreadState, st64.__r13), 0, 1}},
    {"r14b", {0, offsetof(ThreadState, st64.__r14), 0, 1}},
    {"r15b", {0, offsetof(ThreadState, st64.__r15), 0, 1}},


    // segment regs
    {"cs", {offsetof(ThreadState, st32.__cs), offsetof(ThreadState, st64.__cs), 4, 4}},
    {"ds", {offsetof(ThreadState, st32.__ds), 0, 4, 4}},
    {"es", {offsetof(ThreadState, st32.__es), 0, 4, 4}},
    {"fs", {offsetof(ThreadState, st32.__fs), offsetof(ThreadState, st64.__fs), 4, 4}},
    {"gs", {offsetof(ThreadState, st32.__gs), offsetof(ThreadState, st64.__gs), 4, 4}},
    {"ss", {offsetof(ThreadState, st32.__ss), 0, 4, 4}},

    // debug regs
    {"dr0", {offsetof(ThreadState, db32.__dr0), offsetof(ThreadState, db64.__dr0), 4, 8}},
    {"dr1", {offsetof(ThreadState, db32.__dr1), offsetof(ThreadState, db64.__dr1), 4, 8}},
    {"dr2", {offsetof(ThreadState, db32.__dr2), offsetof(ThreadState, db64.__dr2), 4, 8}},
    {"dr3", {offsetof(ThreadState, db32.__dr3), offsetof(ThreadState, db64.__dr3), 4, 8}},
    {"dr4", {offsetof(ThreadState, db32.__dr4), offsetof(ThreadState, db64.__dr4), 4, 8}},
    {"dr5", {offsetof(ThreadState, db32.__dr5), offsetof(ThreadState, db64.__dr5), 4, 8}},
    {"dr6", {offsetof(ThreadState, db32.__dr6), offsetof(ThreadState, db64.__dr6), 4, 8}},
    {"dr7", {offsetof(ThreadState, db32.__dr7), offsetof(ThreadState, db64.__dr7), 4, 8}},

    // instruction pointer
    // pc, ip, eip, rip are all aliases of each other, for safety (can't write
    // just part of the program counter)
    {"pc",  {offsetof(ThreadState, st32.__eip), offsetof(ThreadState, st64.__rip), 4, 8}},
    {"ip",  {offsetof(ThreadState, st32.__eip), offsetof(ThreadState, st64.__rip), 4, 8}},
    {"eip", {offsetof(ThreadState, st32.__eip), offsetof(ThreadState, st64.__rip), 4, 8}},
    {"rip", {offsetof(ThreadState, st32.__eip), offsetof(ThreadState, st64.__rip), 4, 8}},
  };

  try {
    const auto& off = offsets.at(name);

    // find the write offset
    int offset = this->is64 ? off.offset64 : off.offset32;
    int size = this->is64 ? off.size64 : off.size32;
    if (!offset || !size) {
      throw runtime_error(string_printf("writing register %s is not valid in this context", name.c_str()));
    }

    // write to state
    switch (size) {
      case 1:
        *(uint8_t*)((uint8_t*)this + offset) = value;
        break;
      case 2:
        *(uint16_t*)((uint8_t*)this + offset) = value;
        break;
      case 4:
        *(uint32_t*)((uint8_t*)this + offset) = value;
        break;
      case 8:
        *(uint64_t*)((uint8_t*)this + offset) = value;
        break;
    }

  } catch (const out_of_range& e) {
    throw runtime_error(string_printf("register %s does not exist", name.c_str()));
  }
}

ProcessMemoryAdapter::ThreadState ProcessMemoryAdapter::get_thread_registers(
    mach_port_t thread_port) {

  ThreadState st;

  x86_thread_state_t state;
  x86_float_state_t fstate;
  x86_debug_state_t dstate;

  mach_msg_type_number_t count = x86_THREAD_STATE_COUNT;
  kern_return_t ret = thread_get_state(thread_port, x86_THREAD_STATE,
      (thread_state_t)&state, &count);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read int state for thread %d:%d", this->pid, thread_port));
  }
  st.count = state.tsh.count;

  count = x86_FLOAT_STATE_COUNT;
  ret = thread_get_state(thread_port, x86_FLOAT_STATE, (thread_state_t)&fstate, &count);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read float state for thread %d:%d", this->pid, thread_port));
  }
  st.fcount = fstate.fsh.count;

  count = x86_DEBUG_STATE_COUNT;
  ret = thread_get_state(thread_port, x86_DEBUG_STATE, (thread_state_t)&dstate, &count);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t read debug state for thread %d:%d", this->pid, thread_port));
  }
  st.dcount = dstate.dsh.count;

  // add the thread state to our list
  if (state.tsh.flavor == x86_THREAD_STATE32 &&
      fstate.fsh.flavor == x86_FLOAT_STATE32 &&
      dstate.dsh.flavor == x86_DEBUG_STATE32) {
    st.is64 = false;
    st.st32 = state.uts.ts32;
    st.fl32 = fstate.ufs.fs32;
    st.db32 = dstate.uds.ds32;

  } else if (state.tsh.flavor == x86_THREAD_STATE64 &&
             fstate.fsh.flavor == x86_FLOAT_STATE64 &&
             dstate.dsh.flavor == x86_DEBUG_STATE64) {
    st.is64 = true;
    st.st64 = state.uts.ts64;
    st.fl64 = fstate.ufs.fs64;
    st.db64 = dstate.uds.ds64;

  } else {
    throw runtime_error(string_printf("thread state for thread %d:%d has mixed or unknown architecture",
        this->pid, thread_port));
  }

  return st;
}

void ProcessMemoryAdapter::set_thread_registers(mach_port_t thread_port,
    const ThreadState& st) {

  x86_thread_state_t state;
  x86_float_state_t fstate;
  x86_debug_state_t dstate;

  state.tsh.count = st.count;
  fstate.fsh.count = st.fcount;
  dstate.dsh.count = st.dcount;

  if (st.is64) {
    state.tsh.flavor = x86_THREAD_STATE64;
    fstate.fsh.flavor = x86_FLOAT_STATE64;
    dstate.dsh.flavor = x86_DEBUG_STATE64;
    state.uts.ts64 = st.st64;
    fstate.ufs.fs64 = st.fl64;
    dstate.uds.ds64 = st.db64;

  } else {
    state.tsh.flavor = x86_THREAD_STATE32;
    fstate.fsh.flavor = x86_FLOAT_STATE32;
    dstate.dsh.flavor = x86_DEBUG_STATE32;
    state.uts.ts32 = st.st32;
    fstate.ufs.fs32 = st.fl32;
    dstate.uds.ds32 = st.db32;
  }

  kern_return_t ret = thread_set_state(thread_port, x86_THREAD_STATE,
      (thread_state_t)&state, x86_THREAD_STATE_COUNT);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t set int state for thread %d:%d (%d)",
        this->pid, thread_port, ret));
  }

  ret = thread_set_state(thread_port, x86_FLOAT_STATE, (thread_state_t)&fstate,
      x86_FLOAT_STATE_COUNT);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t set float state for thread %d:%d (%d)",
        this->pid, thread_port, ret));
  }

  ret = thread_set_state(thread_port, x86_DEBUG_STATE, (thread_state_t)&dstate,
      x86_DEBUG_STATE_COUNT);
  if (ret != KERN_SUCCESS) {
    throw runtime_error(string_printf("can\'t set debug state for thread %d:%d (%d)",
        this->pid, thread_port, ret));
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


std::unordered_map<mach_port_t, ProcessMemoryAdapter::ThreadState> ProcessMemoryAdapter::get_threads_registers() {

  auto thread_ports_ret = this->get_thread_ports();
  auto& thread_list = thread_ports_ret.first;
  auto& thread_count = thread_ports_ret.second;

  std::unordered_map<mach_port_t, ProcessMemoryAdapter::ThreadState> ret;
  for (size_t x = 0; x < thread_count; x++) {
    auto port = thread_list.get()[x];
    ret.emplace(port, this->get_thread_registers(port));
  }

  return ret;
}

void ProcessMemoryAdapter::set_threads_registers(
    const std::unordered_map<mach_port_t, ProcessMemoryAdapter::ThreadState>& st) {

  auto thread_ports_ret = this->get_thread_ports();
  auto& thread_list = thread_ports_ret.first;
  auto& thread_count = thread_ports_ret.second;

  if (thread_count != st.size()) {
    throw runtime_error(string_printf("incorrect thread count in process %d", this->pid));
  }

  for (size_t x = 0; x < thread_count; x++) {
    auto port = thread_list.get()[x];
    try {
      this->set_thread_registers(port, st.at(port));
    } catch (const out_of_range& e) { }
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
