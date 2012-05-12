// value_test_float.c, by Martin Michelsen, 2012
// simple app that sets its regs to a certain value, then sleeps

.intel_syntax

.globl main
.globl _main
main:
_main:
  xor rax, rax
  mov eax, 0x54389843
  //// rax = 0x0000000054389843

  mov rcx, [rsp]
  //// rcx is an address in the code that called main

  call __geteip
__geteip:
  pop rdx
  //// rdx is like eip

  xor rbx, rbx
1:
  test rbx, rbx
  je 1b // hurr

  // return
  ret
