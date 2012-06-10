// value_test_float.c, by Martin Michelsen, 2012
// simple app that sets its regs to a certain value, then sleeps

.intel_syntax noprefix

.globl main
.globl _main
main:
_main:

  push 0
  push 0
  mov eax, 20
  int 0x80 // getpid()
  push eax
  push 31 // PT_DENY_ATTACH
  mov eax, 26
  push eax
  int 0x80 // ptrace()
  add esp, 20

  mov eax, 0x54389843
  //// eax = 0x0000000054389843

  mov ecx, [esp]
  //// ecx is an address in the code that called main

  call __geteip
__geteip:
  pop edx
  //// edx is like eip

  xor ebx, ebx
1:
  test ebx, ebx
  je 1b // hurr

  mov al, [edx]

  // return
  ret
