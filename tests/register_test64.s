// value_test_float.c, by Martin Michelsen, 2012
// simple app that sets its regs to a certain value, then sleeps

.intel_syntax noprefix

.globl main
.globl _main
main:
_main:

  mov rax, 0x02000014 // getpid
  syscall

  mov rdi, 31 // PT_DENY_ATTACH
  mov rsi, rax // pid
  xor rdx, rdx // NULL
  xor rcx, rcx // 0
  mov rax, 0x0200001A // ptrace
  syscall
  //// ptrace(PT_DENY_ATTACH, getpid(), NULL, 0)

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

  mov al, [rdx]

  // return
  ret
