#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>

#include <phosg/Hash.hh>
#include <phosg/Strings.hh>

#include "AMD64Assembler.hh"
#include "CodeBuffer.hh"

using namespace std;


static void* assemble(CodeBuffer& code, AMD64Assembler& as,
    const char* expected_disassembly = NULL, bool check_disassembly = true) {
  multimap<size_t, string> compiled_labels;
  unordered_set<size_t> patch_offsets;
  string data = as.assemble(patch_offsets, &compiled_labels);
  void* ret = code.append(data, &patch_offsets);

  if (check_disassembly) {
    if (expected_disassembly) {
      string disassembly = AMD64Assembler::disassemble(ret, data.size(), 0,
          &compiled_labels);
      assert(disassembly == expected_disassembly);

    } else {
      print_data(stderr, ret, data.size(), reinterpret_cast<uint64_t>(ret));
      string disassembly = AMD64Assembler::disassemble(ret, data.size(),
        reinterpret_cast<int64_t>(ret), &compiled_labels);
      fprintf(stderr, "%s\n", disassembly.c_str());
      assert(false);
    }
  }

  return ret;
}


void test_trivial_function() {
  printf("-- trivial function\n");

  AMD64Assembler as;
  CodeBuffer code;

  as.write_push(rbp);
  as.write_mov(rbp, rsp);

  as.write_mov(rdx, MemoryReference(rdi, 0));

  as.write_not(rdx);

  as.write_test(rdx, rdx);
  as.write_setz(dh);

  as.write_mov(r10, rdx);

  as.write_test(r10, r10);
  as.write_setz(r10b);

  as.write_xor(r10, 0x3F3F);
  as.write_xor(r10, 0x40);
  as.write_xor(r10b, 0x01, OperandSize::Byte);

  as.write_mov(rax, r10);

  as.write_mov(MemoryReference(rdi, 0), rax);

  as.write_pop(rbp);
  as.write_ret();

  const char* expected_disassembly = "\
0000000000000000   55                              push     rbp\n\
0000000000000001   48 89 E5                        mov      rbp, rsp\n\
0000000000000004   48 8B 17                        mov      rdx, [rdi]\n\
0000000000000007   48 F7 D2                        not      rdx\n\
000000000000000A   48 85 D2                        test     rdx, rdx\n\
000000000000000D   0F 94 C6                        sete     dh\n\
0000000000000010   49 89 D2                        mov      r10, rdx\n\
0000000000000013   4D 85 D2                        test     r10, r10\n\
0000000000000016   41 0F 94 C2                     sete     r10b\n\
000000000000001A   49 81 F2 3F 3F 00 00            xor      r10, 16191\n\
0000000000000021   49 83 F2 40                     xor      r10, 64\n\
0000000000000025   41 80 F2 01                     xor      r10b, 1\n\
0000000000000029   4C 89 D0                        mov      rax, r10\n\
000000000000002C   48 89 07                        mov      [rdi], rax\n\
000000000000002F   5D                              pop      rbp\n\
0000000000000030   C3                              ret\n";

  void* function = assemble(code, as, expected_disassembly);

  uint64_t data = 0x0102030405060708;
  uint64_t (*fn)(uint64_t*) = reinterpret_cast<uint64_t (*)(uint64_t*)>(function);

  // the function should have returned 0xFEFDFCFBFAF93F7E and set the data
  // variable to that value also
  assert(fn(&data) == 0xFEFDFCFBFAF93F7E);
  assert(data == 0xFEFDFCFBFAF93F7E);
}


void test_jump_boundaries() {
  printf("-- jump boundaries\n");

  for (size_t x = 0x70; x < 0x90; x++) {
    AMD64Assembler as;
    CodeBuffer code;

    string nops(x, '\x90');

    as.write_jmp("label1");
    as.write_raw(nops);
    as.write_label("label1");
    as.write_xor(rax, rax);
    as.write_cmp(rax, 0);
    as.write_jz("label2");
    as.write_raw(nops);
    as.write_label("label2");
    as.write_mov(rax, rdi);
    as.write_ret();

    void* function = assemble(code, as, NULL, false);
    int64_t (*fn)(int64_t) = reinterpret_cast<int64_t (*)(int64_t)>(function);

    assert(fn(x) == x);
  }
}


void test_pow() {
  printf("-- pow\n");

  AMD64Assembler as;
  CodeBuffer code;

  // this mirrors the implementation in notes/pow.s
  as.write_mov(rax, 1);
  as.write_label("_pow_again");
  as.write_test(rsi, 1);
  as.write_jz("_pow_skip_base");
  as.write_imul(rax.base_register, rdi);
  as.write_label("_pow_skip_base");
  as.write_imul(rdi.base_register, rdi);
  as.write_shr(rsi, 1);
  as.write_jnz("_pow_again");
  as.write_ret();

  const char* expected_disassembly = "\
0000000000000000   48 C7 C0 01 00 00 00            mov      rax, 0x00000001\n\
_pow_again:\n\
0000000000000007   48 F7 C6 01 00 00 00            test     rsi, 0x00000001\n\
000000000000000E   74 04                           je       +0x4 ; _pow_skip_base\n\
0000000000000010   48 0F AF C7                     imul     rax, rdi\n\
_pow_skip_base:\n\
0000000000000014   48 0F AF FF                     imul     rdi, rdi\n\
0000000000000018   48 D1 EE                        shr      rsi, 1\n\
000000000000001B   75 EA                           jne      -0x16 ; _pow_again\n\
000000000000001D   C3                              ret\n";

  void* function = assemble(code, as, expected_disassembly);
  int64_t (*pow)(int64_t, int64_t) = reinterpret_cast<int64_t (*)(int64_t, int64_t)>(function);

  assert(pow(0, 0) == 1);
  assert(pow(0, 1) == 0);
  assert(pow(0, 10) == 0);
  assert(pow(0, 100) == 0);
  assert(pow(1, 0) == 1);
  assert(pow(1, 1) == 1);
  assert(pow(1, 10) == 1);
  assert(pow(1, 100) == 1);
  assert(pow(2, 0) == 1);
  assert(pow(2, 1) == 2);
  assert(pow(2, 10) == 1024);
  assert(pow(2, 20) == 1048576);
  assert(pow(2, 30) == 1073741824);
  assert(pow(3, 0) == 1);
  assert(pow(3, 1) == 3);
  assert(pow(3, 2) == 9);
  assert(pow(3, 3) == 27);
  assert(pow(3, 4) == 81);
  assert(pow(-1, 0) == 1);
  assert(pow(-1, 1) == -1);
  assert(pow(-1, 2) == 1);
  assert(pow(-1, 3) == -1);
  assert(pow(-1, 4) == 1);
  assert(pow(-2, 0) == 1);
  assert(pow(-2, 1) == -2);
  assert(pow(-2, 10) == 1024);
  assert(pow(-2, 20) == 1048576);
  assert(pow(-2, 30) == 1073741824);
  assert(pow(-3, 0) == 1);
  assert(pow(-3, 1) == -3);
  assert(pow(-3, 2) == 9);
  assert(pow(-3, 3) == -27);
  assert(pow(-3, 4) == 81);
}


void test_quicksort() {
  printf("-- quicksort\n");

  AMD64Assembler as;
  CodeBuffer code;

  // this mirrors the implementation in notes/quicksort.s
  as.write_mov(rdx, rdi);
  as.write_xor(rdi, rdi);
  as.write_dec(rsi);
  as.write_label("0");
  as.write_cmp(rdi, rsi);
  as.write_jl("1");
  as.write_ret();
  as.write_label("1");
  as.write_lea(rcx, MemoryReference(rdi, 0, rsi));
  as.write_shr(rcx, 1);
  as.write_mov(rax, MemoryReference(rdx, 0, rsi, 8));
  as.write_xchg(rax, MemoryReference(rdx, 0, rcx, 8));
  as.write_mov(MemoryReference(rdx, 0, rsi, 8), rax);
  as.write_lea(r8, MemoryReference(rdi, -1));
  as.write_mov(r9, rdi);
  as.write_label("2");
  as.write_inc(r8);
  as.write_cmp(r8, rsi);
  as.write_jge("3");
  as.write_cmp(MemoryReference(rdx, 0, r8, 8), rax);
  as.write_jge("2");
  as.write_mov(rcx, MemoryReference(rdx, 0, r9, 8));
  as.write_xchg(rcx, MemoryReference(rdx, 0, r8, 8));
  as.write_mov(MemoryReference(rdx, 0, r9, 8), rcx);
  as.write_inc(r9);
  as.write_jmp("2");
  as.write_label("3");
  as.write_xchg(rax, MemoryReference(rdx, 0, r9, 8));
  as.write_mov(MemoryReference(rdx, 0, rsi, 8), rax);
  as.write_push(rsi);
  as.write_lea(rax, MemoryReference(r9, 1));
  as.write_push(rax);
  as.write_lea(rsi, MemoryReference(r9, -1));
  as.write_call("0");
  as.write_pop(rdi);
  as.write_pop(rsi);
  as.write_jmp("0");

  const char* expected_disassembly = "\
0000000000000000   48 89 FA                        mov      rdx, rdi\n\
0000000000000003   48 31 FF                        xor      rdi, rdi\n\
0000000000000006   48 FF CE                        dec      rsi\n\
0:\n\
0000000000000009   48 39 F7                        cmp      rdi, rsi\n\
000000000000000C   7C 01                           jl       +0x1 ; 1\n\
000000000000000E   C3                              ret\n\
1:\n\
000000000000000F   48 8D 0C 37                     lea      rcx, [rdi + rsi]\n\
0000000000000013   48 D1 E9                        shr      rcx, 1\n\
0000000000000016   48 8B 04 F2                     mov      rax, [rdx + rsi * 8]\n\
000000000000001A   48 87 04 CA                     xchg     rax, [rdx + rcx * 8]\n\
000000000000001E   48 89 04 F2                     mov      [rdx + rsi * 8], rax\n\
0000000000000022   4C 8D 47 FF                     lea      r8, [rdi - 0x1]\n\
0000000000000026   49 89 F9                        mov      r9, rdi\n\
2:\n\
0000000000000029   49 FF C0                        inc      r8\n\
000000000000002C   49 39 F0                        cmp      r8, rsi\n\
000000000000002F   7D 17                           jge      +0x17 ; 3\n\
0000000000000031   4A 39 04 C2                     cmp      [rdx + r8 * 8], rax\n\
0000000000000035   7D F2                           jge      -0xE ; 2\n\
0000000000000037   4A 8B 0C CA                     mov      rcx, [rdx + r9 * 8]\n\
000000000000003B   4A 87 0C C2                     xchg     rcx, [rdx + r8 * 8]\n\
000000000000003F   4A 89 0C CA                     mov      [rdx + r9 * 8], rcx\n\
0000000000000043   49 FF C1                        inc      r9\n\
0000000000000046   EB E1                           jmp      -0x1F ; 2\n\
3:\n\
0000000000000048   4A 87 04 CA                     xchg     rax, [rdx + r9 * 8]\n\
000000000000004C   48 89 04 F2                     mov      [rdx + rsi * 8], rax\n\
0000000000000050   56                              push     rsi\n\
0000000000000051   49 8D 41 01                     lea      rax, [r9 + 0x1]\n\
0000000000000055   50                              push     rax\n\
0000000000000056   49 8D 71 FF                     lea      rsi, [r9 - 0x1]\n\
000000000000005A   E8 AA FF FF FF                  call     -0x56 ; 0\n\
000000000000005F   5F                              pop      rdi\n\
0000000000000060   5E                              pop      rsi\n\
0000000000000061   EB A6                           jmp      -0x5A ; 0\n";
  void* function = assemble(code, as, expected_disassembly);
  int64_t (*quicksort)(int64_t*, int64_t) = reinterpret_cast<int64_t (*)(int64_t*, int64_t)>(function);

  vector<vector<int64_t>> cases = {
    {},
    {0},
    {6, 4, 2, 0, 3, 1, 7, 9, 8, 5},
    {-100, -10, -1, 0, 1, 10, 100},
    {100, 10, 1, 0, -1, -10, -100},
  };
  for (auto& this_case : cases) {
    int64_t* data = const_cast<int64_t*>(this_case.data());

    quicksort(data, this_case.size());

    fprintf(stderr, "---- sorted:");
    for (uint64_t x = 0; x < this_case.size(); x++) {
      fprintf(stderr, " %" PRId64, data[x]);
    }
    fputc('\n', stderr);

    for (uint64_t x = 1; x < this_case.size(); x++) {
      assert(data[x - 1] < data[x]);
    }
  }
}


void test_hash_fnv1a64() {
  printf("-- hash_fnv1a64\n");

  AMD64Assembler as;
  CodeBuffer code;

  // this mirrors the implementation in notes/hash.s
  as.write_mov(rdx, 0xCBF29CE484222325);
  as.write_add(rsi, rdi);
  as.write_xor(rax, rax);
  as.write_mov(rcx, 0x00000100000001B3);
  as.write_jmp("check_end");

  as.write_label("continue");
  as.write_mov(al, MemoryReference(rdi, 0), OperandSize::Byte);
  as.write_xor(rdx, rax);
  as.write_imul(rdx, rcx);
  as.write_inc(rdi);
  as.write_label("check_end");
  as.write_cmp(rdi, rsi);
  as.write_jne("continue");

  as.write_mov(rax, rdx);
  as.write_ret();

  const char* expected_disassembly = "\
0000000000000000   48 BA 25 23 22 84 E4 9C F2 CB   movabs   rdx, 0xCBF29CE484222325\n\
000000000000000A   48 01 FE                        add      rsi, rdi\n\
000000000000000D   48 31 C0                        xor      rax, rax\n\
0000000000000010   48 B9 B3 01 00 00 00 01 00 00   movabs   rcx, 0x00000100000001B3\n\
000000000000001A   EB 0C                           jmp      +0xC ; check_end\n\
continue:\n\
000000000000001C   8A 07                           mov      al, [rdi]\n\
000000000000001E   48 31 C2                        xor      rdx, rax\n\
0000000000000021   48 0F AF D1                     imul     rdx, rcx\n\
0000000000000025   48 FF C7                        inc      rdi\n\
check_end:\n\
0000000000000028   48 39 F7                        cmp      rdi, rsi\n\
000000000000002B   75 EF                           jne      -0x11 ; continue\n\
000000000000002D   48 89 D0                        mov      rax, rdx\n\
0000000000000030   C3                              ret\n";
  void* function = assemble(code, as, expected_disassembly);
  uint64_t (*hash)(const void*, size_t) = reinterpret_cast<uint64_t (*)(const void*, size_t)>(function);

  assert(hash("", 0) == fnv1a64("", 0));
  assert(hash("omg", 3) == fnv1a64("omg", 3));
  // we intentionally include the \0 at the end of the string here
  assert(hash("0123456789", 11) == fnv1a64("0123456789", 11));
}


void test_float_move_load_multiply() {
  printf("-- floating move + load + multiply\n");

  AMD64Assembler as;
  CodeBuffer code;

  as.write_movq_from_xmm(rax, xmm0);
  as.write_movq_to_xmm(xmm0, rax);
  as.write_movsd(xmm1, MemoryReference(rdi, 0));
  as.write_mulsd(xmm0, xmm1);
  as.write_ret();

  const char* expected_disassembly = "\
0000000000000000   66 48 0F 7E C0                  movq     rax, xmm0\n\
0000000000000005   66 48 0F 6E C0                  movq     xmm0, rax\n\
000000000000000A   F2 0F 10 0F                     movsd    xmm1, [rdi]\n\
000000000000000E   F2 0F 59 C1                     mulsd    xmm0, xmm1\n\
0000000000000012   C3                              ret\n";
  void* function = assemble(code, as, expected_disassembly);
  double (*mul)(double*, double) = reinterpret_cast<double (*)(double*, double)>(function);

  double x = 1.5;
  assert(mul(&x, 3.0) == 4.5);
}


void test_float_neg() {
  printf("-- floating negative\n");

  AMD64Assembler as;
  CodeBuffer code;

  as.write_movq_from_xmm(rax, xmm0);
  as.write_rol(rax, 1);
  as.write_xor(rax, 1);
  as.write_ror(rax, 1);
  as.write_movq_to_xmm(xmm0, rax);
  as.write_ret();

  const char* expected_disassembly = "\
0000000000000000   66 48 0F 7E C0                  movq     rax, xmm0\n\
0000000000000005   48 D1 C0                        rol      rax, 1\n\
0000000000000008   48 83 F0 01                     xor      rax, 1\n\
000000000000000C   48 D1 C8                        ror      rax, 1\n\
000000000000000F   66 48 0F 6E C0                  movq     xmm0, rax\n\
0000000000000014   C3                              ret\n";
  void* function = assemble(code, as, expected_disassembly);
  double (*neg)(double) = reinterpret_cast<double (*)(double)>(function);

  assert(neg(1.5) == -1.5);
}


void test_absolute_patches() {
  printf("-- absolute patches\n");

  AMD64Assembler as;
  CodeBuffer code;

  as.write_mov(rax, "label1");
  as.write_label("label1");
  as.write_ret();

  void* function = assemble(code, as, NULL, false);
  size_t (*fn)() = reinterpret_cast<size_t (*)()>(function);

  // the movabs opcode is 10 bytes long
  assert(fn() == reinterpret_cast<size_t>(function) + 10);
}


int main(int argc, char** argv) {
  test_trivial_function();
  test_jump_boundaries();
  test_pow();
  test_hash_fnv1a64();
  test_quicksort();
  test_float_move_load_multiply();
  test_float_neg();
  test_absolute_patches();

  printf("-- all tests passed\n");
  return 0;
}
