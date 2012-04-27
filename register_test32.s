// value_test_float.c, by Martin Michelsen, 2012
// simple app that sets its regs to a certain value, then sleeps

.intel_syntax

.globl main
.globl _main
main:
_main:
  mov eax, 0x54389843
  //// eax = 0x0000000054389843

  pop ecx
  //// ecx is an address in the code that called main

  call __geteip
__geteip:
  pop edx
  //// edx is like eip

  xor ebx, ebx
1:
  test ebx, ebx
  je 1b // hurr

  // return
  jmp ecx
