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
} VMRegion;

// common regions
extern const VMRegion VMNullRegion; /* <0,0,0> */

// get the number of regions a process has
unsigned int VMCountRegions(pid_t process);
unsigned int VMCountRegionsWithAttributes(pid_t process, unsigned int attribs);

VMRegion VMNextRegion(pid_t process, VMRegion previous);
VMRegion VMNextRegionWithAttributes(pid_t process, VMRegion previous,
                                    unsigned int attribs);

// stop/resume processes
static inline int VMStopProcess(pid_t process)
  { return (kill(process, SIGSTOP) == 0); }
static inline int VMContinueProcess(pid_t process)
  { return (kill(process, SIGCONT) == 0); }

// lower-level reading/writing/protection functions
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

#endif // VMMAP_H
