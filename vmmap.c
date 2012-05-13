// vmmap.c, by Martin Michelsen, 2012
// virtual memory management functions between processes
// adapted from VMRegion; license included below

// VMRegion 0.1
// Virtual Memory Wrapper
//
// Copyright (c) 2004, Charles McGarvey
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "vmmap.h"

#include <mach/mach_init.h> // for current_task
#include <mach/mach_traps.h> // for task_for_pid(3)
#include <mach/task.h> // for task_for_pid(3)
#include <mach/thread_act.h> // for task_for_pid(3)
#include <signal.h> // for stop(2)
#include <stdlib.h> // for stop(2)
#include <stdio.h> // for debugging

#include "parse_utils.h"

static __inline__ vm_map_t _VMTaskFromPID(pid_t process)
{
  vm_map_t task;
  vm_map_t current = current_task();
  if (task_for_pid(current, process, &task) == KERN_SUCCESS)
    return task;
  return 0;
}

static __inline__ VMRegion _VMMakeRegionWithAttributes(pid_t process,
    mach_vm_address_t address, mach_vm_size_t size, unsigned int attribs)
{
  VMRegion region;
  region._process = process;
  region._address = address;
  region._size = size;
  region._attributes = attribs;
  return region;
}

unsigned int _VMAttributesFromAddress(pid_t process, mach_vm_address_t address);

const VMRegion VMNullRegion = { 0, 0, 0, 0 };

VMRegion VMMakeRegion(pid_t process, mach_vm_address_t address,
                      mach_vm_size_t size)
{
  VMRegion region;
  region._process = process;
  region._address = address;
  region._size = size;
  region._attributes = _VMAttributesFromAddress(process, address);
  return region;
}

int VMRegionSetData(VMRegion region, void* data, mach_vm_size_t size)
{
  size = (size > region._size) ? region._size : size;
  return VMWriteBytes(region._process, region._address, data, size);
}

unsigned int VMCountRegions(pid_t process)
{
  VMRegion region, prev = VMNullRegion;
  unsigned int count;

  for (count = 0;
       VMRegionIsNotNull(region = VMNextRegion(process, prev));
       count++)
    prev = region;

  return count;
}

unsigned int VMCountRegionsWithAttributes(pid_t process, unsigned int attribs)
{
  VMRegion region, prev = VMNullRegion;
  unsigned int count;

  for (count = 0;
       VMRegionIsNotNull(region = VMNextRegionWithAttributes(process, prev,
                                                             attribs));
       count++)
    prev = region;

  return count;
}


VMRegion VMNextRegion(pid_t process, VMRegion previous)
{
  vm_map_t task = _VMTaskFromPID(process);
  unsigned int attribs = 0;

  mach_vm_address_t address = 0x0;
  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  if (!VMEqualRegions(previous, VMNullRegion))
    address = previous._address + previous._size;

  // get the next region
  kern_return_t result = mach_vm_region(task, &address, &size,
    VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &infoCnt, &object_name);

  if (result == KERN_SUCCESS) {
    // get the attributes & return the region
    if (info.protection & VM_PROT_READ)
      attribs |= VMREGION_READABLE;
    if (info.protection & VM_PROT_WRITE)
      attribs |= VMREGION_WRITABLE;
    if (info.protection & VM_PROT_EXECUTE)
      attribs |= VMREGION_EXECUTABLE;
    return _VMMakeRegionWithAttributes(process, address, size, attribs);
  }

  return VMNullRegion;
}

VMRegion VMNextRegionWithAttributes(pid_t process, VMRegion previous,
                                    unsigned int attribs)
{
  VMRegion region;

  while (VMRegionIsNotNull(region = VMNextRegion(process, previous)))
  {
    if ((attribs & region._attributes) == attribs)
      return region;
    previous = region;
  }

  return VMNullRegion;
}



int VMSetRegionProtection(pid_t process, mach_vm_address_t address,
                          mach_vm_size_t size, int prot_flags, int prot_mask) {

  vm_map_t task = _VMTaskFromPID(process);

  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get the next region
  kern_return_t result = mach_vm_region(task, &address, &size,
    VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &infoCnt, &object_name);

  if (result != KERN_SUCCESS)
    return 0;

  // get the attributes and return them
  if (prot_mask & VMREGION_READABLE) {
    info.protection &= ~VM_PROT_READ;
    if (prot_flags & VMREGION_READABLE)
      info.protection |= VM_PROT_READ;
  }
  if (prot_mask & VMREGION_WRITABLE) {
    info.protection &= ~VM_PROT_WRITE;
    if (prot_flags & VMREGION_WRITABLE)
      info.protection |= VM_PROT_WRITE;
  }
  if (prot_mask & VMREGION_EXECUTABLE) {
    info.protection &= ~VM_PROT_EXECUTE;
    if (prot_flags & VMREGION_EXECUTABLE)
      info.protection |= VM_PROT_EXECUTE;
  }

  result = mach_vm_protect(task, address, size, 0, info.protection);
  if (result != KERN_SUCCESS)
    return 0;
  return 1;
}

int VMReadBytes(pid_t process, mach_vm_address_t address, void *bytes,
                mach_vm_size_t *size)
{
  vm_map_t task = _VMTaskFromPID(process);
  mach_vm_size_t staticsize = *size;
  
  // perform the read
  kern_return_t result = mach_vm_read_overwrite(task, address, staticsize,
                                                (vm_offset_t)bytes, size);
  if (result != KERN_SUCCESS)
    return 0;
  return 1;
}

int VMWriteBytes(pid_t process, mach_vm_address_t address, const void *bytes,
                 mach_vm_size_t size)
{
  vm_map_t task = _VMTaskFromPID(process);
  kern_return_t result = mach_vm_write(task, address, (vm_offset_t)bytes, size);
  return (result == KERN_SUCCESS);
}

unsigned int _VMAttributesFromAddress(pid_t process, mach_vm_address_t address)
{
  vm_map_t task = _VMTaskFromPID(process);
  unsigned int attribs = 0;

  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get the next region
  kern_return_t result = mach_vm_region(task, &address, &size,
    VM_REGION_BASIC_INFO_64, (vm_region_info_t)(&info), &infoCnt, &object_name);

  if (result == KERN_SUCCESS) {
    // get the attributes and return them
    if (info.protection & VM_PROT_READ)
      attribs |= VMREGION_READABLE;
    if (info.protection & VM_PROT_WRITE)
      attribs |= VMREGION_WRITABLE;
    if (info.protection & VM_PROT_EXECUTE)
      attribs |= VMREGION_EXECUTABLE;
    return attribs;
  }
  return 0;
}



// suspends a process using the Mach API
int VMPauseProcess(pid_t pid) {
  vm_map_t task = _VMTaskFromPID(pid);
  kern_return_t result = task_suspend(task);
  return (result == KERN_SUCCESS);
}

// resumes a process using the Mach API
int VMResumeProcess(pid_t pid) {
  vm_map_t task = _VMTaskFromPID(pid);
  kern_return_t result = task_resume(task);
  return (result == KERN_SUCCESS);
}



// prints the register contents in a thread state
void VMPrintThreadRegisters(VMThreadState* state) {
  
  if (!state->is64) {
    printf("[thread 32-bit @ eip:%08X]\n", state->st32.__eip);
    printf("  eax: %08X  ecx: %08X  edx: %08X  ebx: %08X\n", state->st32.__eax,
           state->st32.__ecx, state->st32.__edx, state->st32.__ebx);
    printf("  ebp: %08X  esp: %08X  esi: %08X  edi: %08X\n", state->st32.__ebp,
           state->st32.__esp, state->st32.__esi, state->st32.__edi);
    printf("  eflags: %08X\n", state->st32.__eflags);
    printf("  cs:  %08X  ds:  %08X  es:  %08X  fs:  %08X\n", state->st32.__cs,
           state->st32.__ds, state->st32.__es, state->st32.__fs);
    printf("  gs:  %08X  ss:  %08X\n", state->st32.__gs, state->st32.__ss);
    // TODO: print floating state
    printf("  dr0: %08X  dr1: %08X  dr2: %08X  dr3: %08X\n", state->db32.__dr0,
           state->db32.__dr1, state->db32.__dr2, state->db32.__dr3);
    printf("  dr4: %08X  dr5: %08X  dr6: %08X  dr7: %08X\n", state->db32.__dr4,
           state->db32.__dr5, state->db32.__dr6, state->db32.__dr7);
  } else {
    printf("[thread 64-bit @ rip:%016llX]\n", state->st64.__rip);
    printf("  rax: %016llX  rcx: %016llX  rdx: %016llX\n", state->st64.__rax,
           state->st64.__rcx, state->st64.__rdx);
    printf("  rbx: %016llX  rbp: %016llX  rsp: %016llX\n", state->st64.__rbx,
           state->st64.__rbp, state->st64.__rsp);
    printf("  rsi: %016llX  rdi: %016llX  r8:  %016llX\n", state->st64.__rsi,
           state->st64.__rdi, state->st64.__r8);
    printf("  r9:  %016llX  r10: %016llX  r11: %016llX\n", state->st64.__r9,
           state->st64.__r10, state->st64.__r11);
    printf("  r12: %016llX  r13: %016llX  r14: %016llX\n", state->st64.__r12,
           state->st64.__r13, state->st64.__r14);
    printf("  r15: %016llX  rflags: %016llX\n", state->st64.__r15,
           state->st64.__rflags);
    printf("  cs:  %016llX  fs:  %016llX  gs:  %016llX\n", state->st64.__cs,
           state->st64.__fs, state->st64.__gs);
    // TODO: print floating state
    printf("  dr0: %016llX  dr1: %016llX  dr2: %016llX\n", state->db64.__dr0,
           state->db64.__dr1, state->db64.__dr2);
    printf("  dr3: %016llX  dr4: %016llX  dr5: %016llX\n", state->db64.__dr3,
           state->db64.__dr4, state->db64.__dr5);
    printf("  dr6: %016llX  dr7: %016llX\n", state->db64.__dr6,
           state->db64.__dr7);
  }
}

#define offsetof(st, m) __builtin_offsetof(st, m)

// sets a regiter value in the given state by name
int VMSetRegisterValueByName(VMThreadState* state, const char* name,
                             uint64_t value) {

  static const struct {
    char name[8];
    int offset32;
    int offset64;
    int size32;
    int size64;
  } registers[] = {

    // 64-bit regs
    {"rax", 0, offsetof(VMThreadState, st64.__rax), 0, 8},
    {"rcx", 0, offsetof(VMThreadState, st64.__rcx), 0, 8},
    {"rdx", 0, offsetof(VMThreadState, st64.__rdx), 0, 8},
    {"rbx", 0, offsetof(VMThreadState, st64.__rbx), 0, 8},
    {"rsp", 0, offsetof(VMThreadState, st64.__rsp), 0, 8},
    {"rbp", 0, offsetof(VMThreadState, st64.__rbp), 0, 8},
    {"rsi", 0, offsetof(VMThreadState, st64.__rsi), 0, 8},
    {"rdi", 0, offsetof(VMThreadState, st64.__rdi), 0, 8},
    {"r8",  0, offsetof(VMThreadState, st64.__r8), 0, 8},
    {"r9",  0, offsetof(VMThreadState, st64.__r9), 0, 8},
    {"r10", 0, offsetof(VMThreadState, st64.__r10), 0, 8},
    {"r11", 0, offsetof(VMThreadState, st64.__r11), 0, 8},
    {"r12", 0, offsetof(VMThreadState, st64.__r12), 0, 8},
    {"r13", 0, offsetof(VMThreadState, st64.__r13), 0, 8},
    {"r14", 0, offsetof(VMThreadState, st64.__r14), 0, 8},
    {"r15", 0, offsetof(VMThreadState, st64.__r15), 0, 8},
    {"rflags", 0, offsetof(VMThreadState, st64.__rflags), 0, 8},

    // 32-bit regs
    {"eax", offsetof(VMThreadState, st32.__eax),
            offsetof(VMThreadState, st64.__rax), 4, 4},
    {"ecx", offsetof(VMThreadState, st32.__ecx),
            offsetof(VMThreadState, st64.__rcx), 4, 4},
    {"edx", offsetof(VMThreadState, st32.__edx),
            offsetof(VMThreadState, st64.__rdx), 4, 4},
    {"ebx", offsetof(VMThreadState, st32.__ebx),
            offsetof(VMThreadState, st64.__rbx), 4, 4},
    {"esp", offsetof(VMThreadState, st32.__esp),
            offsetof(VMThreadState, st64.__rsp), 4, 4},
    {"ebp", offsetof(VMThreadState, st32.__ebp),
            offsetof(VMThreadState, st64.__rbp), 4, 4},
    {"esi", offsetof(VMThreadState, st32.__esi),
            offsetof(VMThreadState, st64.__rsi), 4, 4},
    {"edi", offsetof(VMThreadState, st32.__edi),
            offsetof(VMThreadState, st64.__rdi), 4, 4},
    {"r8d",  0, offsetof(VMThreadState, st64.__r8),  0, 4},
    {"r9d",  0, offsetof(VMThreadState, st64.__r9),  0, 4},
    {"r10d", 0, offsetof(VMThreadState, st64.__r10), 0, 4},
    {"r11d", 0, offsetof(VMThreadState, st64.__r11), 0, 4},
    {"r12d", 0, offsetof(VMThreadState, st64.__r12), 0, 4},
    {"r13d", 0, offsetof(VMThreadState, st64.__r13), 0, 4},
    {"r14d", 0, offsetof(VMThreadState, st64.__r14), 0, 4},
    {"r15d", 0, offsetof(VMThreadState, st64.__r15), 0, 4},
    {"eflags", offsetof(VMThreadState, st32.__eflags),
               offsetof(VMThreadState, st64.__rflags), 4, 4},

    // 16-bit regs
    {"ax", offsetof(VMThreadState, st32.__eax),
           offsetof(VMThreadState, st64.__rax), 2, 2},
    {"cx", offsetof(VMThreadState, st32.__ecx),
           offsetof(VMThreadState, st64.__rcx), 2, 2},
    {"dx", offsetof(VMThreadState, st32.__edx),
           offsetof(VMThreadState, st64.__rdx), 2, 2},
    {"bx", offsetof(VMThreadState, st32.__ebx),
           offsetof(VMThreadState, st64.__rbx), 2, 2},
    {"sp", offsetof(VMThreadState, st32.__esp),
           offsetof(VMThreadState, st64.__rsp), 2, 2},
    {"bp", offsetof(VMThreadState, st32.__ebp),
           offsetof(VMThreadState, st64.__rbp), 2, 2},
    {"si", offsetof(VMThreadState, st32.__esi),
           offsetof(VMThreadState, st64.__rsi), 2, 2},
    {"di", offsetof(VMThreadState, st32.__edi),
           offsetof(VMThreadState, st64.__rdi), 2, 2},
    {"r8w",  0, offsetof(VMThreadState, st64.__r8),  0, 2},
    {"r9w",  0, offsetof(VMThreadState, st64.__r9),  0, 2},
    {"r10w", 0, offsetof(VMThreadState, st64.__r10), 0, 2},
    {"r11w", 0, offsetof(VMThreadState, st64.__r11), 0, 2},
    {"r12w", 0, offsetof(VMThreadState, st64.__r12), 0, 2},
    {"r13w", 0, offsetof(VMThreadState, st64.__r13), 0, 2},
    {"r14w", 0, offsetof(VMThreadState, st64.__r14), 0, 2},
    {"r15w", 0, offsetof(VMThreadState, st64.__r15), 0, 2},
    {"flags", offsetof(VMThreadState, st32.__eflags),
              offsetof(VMThreadState, st64.__rflags), 2, 2},

    // 8-bit regs
    {"al", offsetof(VMThreadState, st32.__eax),
           offsetof(VMThreadState, st64.__rax), 1, 1},
    {"cl", offsetof(VMThreadState, st32.__ecx),
           offsetof(VMThreadState, st64.__rcx), 1, 1},
    {"dl", offsetof(VMThreadState, st32.__edx),
           offsetof(VMThreadState, st64.__rdx), 1, 1},
    {"bl", offsetof(VMThreadState, st32.__ebx),
           offsetof(VMThreadState, st64.__rbx), 1, 1},
    {"ah", offsetof(VMThreadState, st32.__eax) + 1,
           offsetof(VMThreadState, st64.__rax) + 1, 1, 1},
    {"ch", offsetof(VMThreadState, st32.__ecx) + 1,
           offsetof(VMThreadState, st64.__rcx) + 1, 1, 1},
    {"dh", offsetof(VMThreadState, st32.__edx) + 1,
           offsetof(VMThreadState, st64.__rdx) + 1, 1, 1},
    {"bh", offsetof(VMThreadState, st32.__ebx) + 1,
           offsetof(VMThreadState, st64.__rbx) + 1, 1, 1},
    {"spl", offsetof(VMThreadState, st32.__esp),
            offsetof(VMThreadState, st64.__rsp), 1, 1},
    {"bpl", offsetof(VMThreadState, st32.__ebp),
            offsetof(VMThreadState, st64.__rbp), 1, 1},
    {"sil", offsetof(VMThreadState, st32.__esi),
            offsetof(VMThreadState, st64.__rsi), 1, 1},
    {"dil", offsetof(VMThreadState, st32.__edi),
            offsetof(VMThreadState, st64.__rdi), 1, 1},
    {"r8b",  0, offsetof(VMThreadState, st64.__r8),  0, 1},
    {"r9b",  0, offsetof(VMThreadState, st64.__r9),  0, 1},
    {"r10b", 0, offsetof(VMThreadState, st64.__r10), 0, 1},
    {"r11b", 0, offsetof(VMThreadState, st64.__r11), 0, 1},
    {"r12b", 0, offsetof(VMThreadState, st64.__r12), 0, 1},
    {"r13b", 0, offsetof(VMThreadState, st64.__r13), 0, 1},
    {"r14b", 0, offsetof(VMThreadState, st64.__r14), 0, 1},
    {"r15b", 0, offsetof(VMThreadState, st64.__r15), 0, 1},


    // segment regs
    {"cs", offsetof(VMThreadState, st32.__cs),
           offsetof(VMThreadState, st64.__cs), 4, 4},
    {"ds", offsetof(VMThreadState, st32.__ds), 0, 4, 4},
    {"es", offsetof(VMThreadState, st32.__es), 0, 4, 4},
    {"fs", offsetof(VMThreadState, st32.__fs),
           offsetof(VMThreadState, st64.__fs), 4, 4},
    {"gs", offsetof(VMThreadState, st32.__gs),
           offsetof(VMThreadState, st64.__gs), 4, 4},
    {"ss", offsetof(VMThreadState, st32.__ss), 0, 4, 4},

    // debug regs
    {"dr0", offsetof(VMThreadState, db32.__dr0),
            offsetof(VMThreadState, db64.__dr0), 4, 8},
    {"dr1", offsetof(VMThreadState, db32.__dr1),
            offsetof(VMThreadState, db64.__dr1), 4, 8},
    {"dr2", offsetof(VMThreadState, db32.__dr2),
            offsetof(VMThreadState, db64.__dr2), 4, 8},
    {"dr3", offsetof(VMThreadState, db32.__dr3),
            offsetof(VMThreadState, db64.__dr3), 4, 8},
    {"dr4", offsetof(VMThreadState, db32.__dr4),
            offsetof(VMThreadState, db64.__dr4), 4, 8},
    {"dr5", offsetof(VMThreadState, db32.__dr5),
            offsetof(VMThreadState, db64.__dr5), 4, 8},
    {"dr6", offsetof(VMThreadState, db32.__dr6),
            offsetof(VMThreadState, db64.__dr6), 4, 8},
    {"dr7", offsetof(VMThreadState, db32.__dr7),
            offsetof(VMThreadState, db64.__dr7), 4, 8},

    // instruction pointer
    // pc, ip, eip, rip are all aliases of each other, for safety (can't write
    // just part of the program counter)
    {"pc",  offsetof(VMThreadState, st32.__eip),
            offsetof(VMThreadState, st64.__rip), 4, 8},
    {"ip",  offsetof(VMThreadState, st32.__eip),
            offsetof(VMThreadState, st64.__rip), 4, 8},
    {"eip", offsetof(VMThreadState, st32.__eip),
            offsetof(VMThreadState, st64.__rip), 4, 8},
    {"rip", offsetof(VMThreadState, st32.__eip),
            offsetof(VMThreadState, st64.__rip), 4, 8},

    // no more registers...
    {"", 0, 0, 0}};

  // find the named register
  int x;
  for (x = 0; registers[x].name[0]; x++)
    if (!strcmp(registers[x].name, name))
      break;
  if (!registers[x].name[0])
    return -1;

  // find the write offset
  int offset = state->is64 ? registers[x].offset64 : registers[x].offset32;
  int size = state->is64 ? registers[x].size64 : registers[x].size32;
  if (!offset || !size)
    return -2;

  // write to state
  switch (size) {
    case 1:
      *(uint8_t*)((uint8_t*)state + offset) = value;
      break;
    case 2:
      *(uint16_t*)((uint8_t*)state + offset) = value;
      break;
    case 4:
      *(uint32_t*)((uint8_t*)state + offset) = value;
      break;
    case 8:
      *(uint64_t*)((uint8_t*)state + offset) = value;
      break;
    default:
      return (-3);
  }

  return 0;
}

// gets thread registers for all threads in the process
// return: number of threads if > 0
// return: < 0 if error
// the states must be free()'d when no longer required.
int VMGetThreadRegisters(pid_t process, VMThreadState** states) {

  // get the task port
  vm_map_t task = _VMTaskFromPID(process);
  if (!task)
    return -1;

  // get the thread list
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  if (task_threads(task, &thread_list, &thread_count) != KERN_SUCCESS)
    return -2;

  // no threads? return NULL and 0
  if (!thread_count) {
    *states = NULL;
    return 0;
  }
  
  // alloc the thread info list
  int error = 0;
  *states = (VMThreadState*)malloc(sizeof(VMThreadState) * thread_count);
  if (!*states) {
    error = -3;
    goto VMGetThreadRegisters_cleanup;
  }

  // for each thread...
  int x;
  for (x = 0; x < thread_count; x++) {

    // get the thread state
    x86_thread_state_t state;
    x86_float_state_t fstate;
    x86_debug_state_t dstate;

    mach_msg_type_number_t sc = x86_THREAD_STATE_COUNT;
    if (thread_get_state(thread_list[x], x86_THREAD_STATE,
                         (thread_state_t)&state, &sc) != KERN_SUCCESS) {
      free(*states);
      error = -4;
      goto VMGetThreadRegisters_cleanup;
    }
    (*states)[x].count = state.tsh.count;

    sc = x86_FLOAT_STATE_COUNT;
    if (thread_get_state(thread_list[x], x86_FLOAT_STATE,
                         (thread_state_t)&fstate, &sc) != KERN_SUCCESS) {
      free(*states);
      error = -5;
      goto VMGetThreadRegisters_cleanup;
    }
    (*states)[x].fcount = fstate.fsh.count;

    sc = x86_THREAD_STATE_COUNT;
    if (thread_get_state(thread_list[x], x86_DEBUG_STATE,
                         (thread_state_t)&dstate, &sc) != KERN_SUCCESS) {
      free(*states);
      error = -6;
      goto VMGetThreadRegisters_cleanup;
    }
    (*states)[x].dcount = dstate.dsh.count;

    // add the thread state to our list
    if (state.tsh.flavor == x86_THREAD_STATE32 &&
        fstate.fsh.flavor == x86_FLOAT_STATE32 &&
        dstate.dsh.flavor == x86_DEBUG_STATE32) {
      (*states)[x].is64 = 0;
      memcpy(&(*states)[x].st32, &state.uts.ts32, sizeof(x86_thread_state32_t));
      memcpy(&(*states)[x].fl32, &fstate.ufs.fs32, sizeof(x86_float_state32_t));
      memcpy(&(*states)[x].db32, &dstate.uds.ds32, sizeof(x86_debug_state32_t));
    } else if (state.tsh.flavor == x86_THREAD_STATE64 &&
               fstate.fsh.flavor == x86_FLOAT_STATE64 &&
               dstate.dsh.flavor == x86_DEBUG_STATE64) {
      (*states)[x].is64 = 1;
      memcpy(&(*states)[x].st64, &state.uts.ts64, sizeof(x86_thread_state64_t));
      memcpy(&(*states)[x].fl64, &fstate.ufs.fs64, sizeof(x86_float_state64_t));
      memcpy(&(*states)[x].db64, &dstate.uds.ds64, sizeof(x86_debug_state64_t));
    } else {
      free(*states);
      error = -7;
      goto VMGetThreadRegisters_cleanup;
    }
  }

VMGetThreadRegisters_cleanup:
  if (error)
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  (thread_count * sizeof (int)));
  else
    error = vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                          (thread_count * sizeof (int)));

  return thread_count;
}

/*int VMSetThreadRegisters(pid_t process, const VMThreadState* state) {

  // use set_thread_state, like this:
  thread_set_state(thread_list[thread], PPC_THREAD_STATE,
                   (thread_state_t)&ppc_state, sc);
  return 0;
} */

// sets thread registers for all threads in the process
// return: number of threads if > 0
// return: < 0 if error
// the states must be free()'d when no longer required.
int VMSetThreadRegisters(pid_t process, const VMThreadState* states, int num) {

  // get the task port
  vm_map_t task = _VMTaskFromPID(process);
  if (!task)
    return -1;

  // get the thread list
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  if (task_threads(task, &thread_list, &thread_count) != KERN_SUCCESS)
    return -2;

  // no threads? return NULL and 0
  if (!thread_count || (thread_count != num))
    return -3;
  
  // for each thread...
  int x, error = 0;
  x86_thread_state_t state;
  x86_float_state_t fstate;
  x86_debug_state_t dstate;
  for (x = 0; x < thread_count; x++) {

    // prepare the thread state
    state.tsh.count = states[x].count;
    fstate.fsh.count = states[x].fcount;
    dstate.dsh.count = states[x].dcount;
    if (states[x].is64) {
      state.tsh.flavor = x86_THREAD_STATE64;
      fstate.fsh.flavor = x86_FLOAT_STATE64;
      dstate.dsh.flavor = x86_DEBUG_STATE64;
      memcpy(&state.uts.ts64, &states[x].st64, sizeof(x86_thread_state64_t));
      memcpy(&fstate.ufs.fs64, &states[x].fl64, sizeof(x86_float_state64_t));
      memcpy(&dstate.uds.ds64, &states[x].db64, sizeof(x86_debug_state64_t));
    } else {
      state.tsh.flavor = x86_THREAD_STATE32;
      fstate.fsh.flavor = x86_FLOAT_STATE32;
      dstate.dsh.flavor = x86_DEBUG_STATE32;
      memcpy(&state.uts.ts32, &states[x].st32, sizeof(x86_thread_state32_t));
      memcpy(&fstate.ufs.fs32, &states[x].fl32, sizeof(x86_float_state32_t));
      memcpy(&dstate.uds.ds32, &states[x].db32, sizeof(x86_debug_state32_t));
    }

    // set the thread state
    error = thread_set_state(thread_list[x], x86_THREAD_STATE,
        (thread_state_t)&state, x86_THREAD_STATE_COUNT);
    if (error != KERN_SUCCESS)
      goto VMSetThreadRegisters_cleanup;

    // set the floating state
    error = thread_set_state(thread_list[x], x86_FLOAT_STATE,
        (thread_state_t)&fstate, x86_FLOAT_STATE_COUNT);
    if (error != KERN_SUCCESS)
      goto VMSetThreadRegisters_cleanup;

    // set the debug state
    error = thread_set_state(thread_list[x], x86_DEBUG_STATE,
        (thread_state_t)&dstate, x86_DEBUG_STATE_COUNT);
    if (error != KERN_SUCCESS)
      goto VMSetThreadRegisters_cleanup;
  }

VMSetThreadRegisters_cleanup:
  if (error)
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  (thread_count * sizeof (int)));
  else
    error = vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                          (thread_count * sizeof (int)));
  return 0;
}
