// vmmap.h, by Martin Michelsen, 2012
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

#ifndef VMMAP_H
#define VMMAP_H

#include <sys/types.h>
#include <signal.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>

// attributes of memory regions
enum _VMRegionAttributes {
  VMREGION_READABLE = 1,
  VMREGION_WRITABLE = 2,
  VMREGION_EXECUTABLE = 4,
  VMREGION_ALL = 7,
};

typedef struct _VMRegion {
  pid_t _process;
  mach_vm_address_t _address;
  mach_vm_size_t _size;
  unsigned int _attributes;
  unsigned int _max_attributes;
  int _inherit;
  int _shared;
  int _reserved;
  vm_behavior_t _behavior;
  unsigned short _user_wired_count;
} VMRegion;

// common regions
extern const VMRegion VMNullRegion; /* <0,0,0> */

// get the number of regions a process has
unsigned int VMCountRegions(pid_t process);
unsigned int VMCountRegionsWithAttributes(pid_t process, unsigned int attribs);

VMRegion VMNextRegion(pid_t process, VMRegion previous);
VMRegion VMNextRegionWithAttributes(pid_t process, VMRegion previous,
                                    unsigned int attribs);

// lower-level reading/writing/protection functions
void* VMAlloc(size_t size);
int VMFree(void* addr, size_t size);
int VMAllocateRegion(pid_t process, vm_address_t address,
                     mach_vm_size_t size);
int VMDeallocateRegion(pid_t process, vm_address_t address,
                       mach_vm_size_t size);
int VMSetRegionProtection(pid_t process, mach_vm_address_t address,
                          mach_vm_size_t size, int prot_flags, int prot_mask);
int VMReadBytes(pid_t process, mach_vm_address_t address, void *bytes,
                mach_vm_size_t *size); // size is # bytes read after call
int VMWriteBytes(pid_t process, mach_vm_address_t address, const void *bytes,
                 mach_vm_size_t size);

extern VMRegion VMMakeRegion(pid_t process, mach_vm_address_t address,
                             mach_vm_size_t size);
extern int VMRegionSetData(VMRegion region, void* data, mach_vm_size_t size);

static inline mach_vm_address_t VMRegionProcess(VMRegion region)
  { return region._process; }
static inline mach_vm_address_t VMRegionAddress(VMRegion region)
  { return region._address; }
static inline mach_vm_size_t VMRegionSize(VMRegion region)
  { return region._size; }
static inline unsigned int VMRegionAttributes(VMRegion region)
  { return region._attributes; }
static inline int VMRegionReadable(VMRegion region)
  { return region._attributes & VMREGION_READABLE; }
static inline int VMRegionWritable(VMRegion region)
  { return region._attributes & VMREGION_WRITABLE; }
static inline int VMRegionExecutable(VMRegion region)
  { return region._attributes & VMREGION_EXECUTABLE; }
static inline int VMAddressInRegion(VMRegion rgn, mach_vm_address_t addr)
  { return (addr >= rgn._address) && (addr < rgn._address + rgn._size); }

static inline int VMRegionBytes(VMRegion region, void *bytes,
                                mach_vm_size_t *size) {
  *size = region._size;
  return VMReadBytes(region._process, region._address, bytes, size);
}

static inline int VMRegionIsNotNull(VMRegion region)
  { return (region._process != 0); }
static inline int VMEqualRegions(VMRegion region1, VMRegion region2)
  { return (region1._process == region2._process &&
            region1._address == region2._address &&
            region1._size == region2._size &&
            region1._attributes == region2._attributes); }



int VMPauseProcess(pid_t pid);
int VMResumeProcess(pid_t pid);
int VMTerminateProcess(pid_t pid);



typedef struct {
  int is64;
  mach_msg_type_number_t count, fcount, dcount;
  int tid;
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
} VMThreadState;

void VMPrintThreadRegisters(VMThreadState* state, int thread_id);
int VMSetRegisterValueByName(VMThreadState* state, const char* name,
                             uint64_t value);
int VMGetThreadRegisters(mach_port_t thread_port, VMThreadState* _state);
int VMSetThreadRegisters(mach_port_t thread_port, const VMThreadState* _state);
int VMGetProcessRegisters(pid_t process, VMThreadState** state);
int VMSetProcessRegisters(pid_t process, const VMThreadState* state, int num);

int VMWaitForBreakpoint(pid_t pid,
                        int (*handler)(mach_port_t, int, VMThreadState*));

#endif // VMMAP_H
