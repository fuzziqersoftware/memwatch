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
#include <signal.h> // for stop(2)

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

  kern_return_t result;

  mach_vm_address_t address = 0x0;
  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  if (!VMEqualRegions(previous, VMNullRegion))
    address = previous._address + previous._size;

  // get the next region
  result = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                          (vm_region_info_t)(&info), &infoCnt, &object_name);

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

  kern_return_t result;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get the next region
  result = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                          (vm_region_info_t)(&info), &infoCnt, &object_name);

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
  kern_return_t result;
  mach_vm_size_t staticsize = *size;
  
  // perform the read
  result = mach_vm_read_overwrite(task, address, staticsize,
                                  (vm_offset_t)bytes, size);
  if (result != KERN_SUCCESS)
    return 0;
  return 1;
}

int VMWriteBytes(pid_t process, mach_vm_address_t address, const void *bytes,
                 mach_vm_size_t size)
{
  vm_map_t task = _VMTaskFromPID(process);
  kern_return_t result;

  // attempt to write the bytes and return success/failure
  result = mach_vm_write(task, address, (vm_offset_t)bytes, size);
  return (result == KERN_SUCCESS);
}

unsigned int _VMAttributesFromAddress(pid_t process, mach_vm_address_t address)
{
  vm_map_t task = _VMTaskFromPID(process);
  unsigned int attribs = 0;

  kern_return_t result;
  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = 0;

  // get the next region
  result = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                          (vm_region_info_t)(&info), &infoCnt, &object_name);

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
