#pragma once

#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>


enum Operation {
  ADD_STORE8 = 0x00,
  ADD_STORE  = 0x01,
  ADD_LOAD8  = 0x02,
  ADD_LOAD   = 0x03,
  OR_STORE8  = 0x08,
  OR_STORE   = 0x09,
  OR_LOAD8   = 0x0A,
  OR_LOAD    = 0x0B,
  ADC_STORE8 = 0x10,
  ADC_STORE  = 0x11,
  ADC_LOAD8  = 0x12,
  ADC_LOAD   = 0x13,
  SBB_STORE8 = 0x18,
  SBB_STORE  = 0x19,
  SBB_LOAD8  = 0x1A,
  SBB_LOAD   = 0x1B,
  AND_STORE8 = 0x20,
  AND_STORE  = 0x21,
  AND_LOAD8  = 0x22,
  AND_LOAD   = 0x23,
  SUB_STORE8 = 0x28,
  SUB_STORE  = 0x29,
  SUB_LOAD8  = 0x2A,
  SUB_LOAD   = 0x2B,
  XOR_STORE8 = 0x30,
  XOR_STORE  = 0x31,
  XOR_LOAD8  = 0x32,
  XOR_LOAD   = 0x33,
  CMP_STORE8 = 0x38,
  CMP_STORE  = 0x39,
  CMP_LOAD8  = 0x3A,
  CMP_LOAD   = 0x3B,
  REX        = 0x40,
  REX_B      = 0x41,
  REX_X      = 0x42,
  REX_XB     = 0x43,
  REX_R      = 0x44,
  REX_RB     = 0x45,
  REX_RX     = 0x46,
  REX_RXB    = 0x47,
  REX_W      = 0x48,
  REX_WB     = 0x49,
  REX_WX     = 0x4A,
  REX_WXB    = 0x4B,
  REX_WR     = 0x4C,
  REX_WRB    = 0x4D,
  REX_WRX    = 0x4E,
  REX_WRXB   = 0x4F,
  OPERAND16  = 0x66,
  PUSH32     = 0x68,
  PUSH8      = 0x6A,
  JO8        = 0x70,
  JNO8       = 0x71,
  JB8        = 0x72,
  JNAE8      = 0x72,
  JC8        = 0x72,
  JNB8       = 0x73,
  JAE8       = 0x73,
  JNC8       = 0x73,
  JZ8        = 0x74,
  JE8        = 0x74,
  JNZ8       = 0x75,
  JNE8       = 0x75,
  JBE8       = 0x76,
  JNA8       = 0x76,
  JNBE8      = 0x77,
  JA8        = 0x77,
  JS8        = 0x78,
  JNS8       = 0x79,
  JP8        = 0x7A,
  JPE8       = 0x7A,
  JNP8       = 0x7B,
  JPO8       = 0x7B,
  JL8        = 0x7C,
  JNGE8      = 0x7C,
  JNL8       = 0x7D,
  JGE8       = 0x7D,
  JLE8       = 0x7E,
  JNG8       = 0x7E,
  JNLE8      = 0x7F,
  JG8        = 0x7F,
  MATH8_IMM8 = 0x80,
  MATH_IMM32 = 0x81,
  MATH_IMM8  = 0x83,
  TEST8      = 0x84,
  TEST       = 0x85,
  XCHG8      = 0x86,
  XCHG       = 0x87,
  MOV_STORE8 = 0x88,
  MOV_STORE  = 0x89,
  MOV_LOAD8  = 0x8A,
  MOV_LOAD   = 0x8B,
  LEA        = 0x8D,
  POP_RM     = 0x8F,
  NOP        = 0x90,
  SHIFT8_IMM = 0xC0,
  SHIFT_IMM  = 0xC1,
  RET_IMM    = 0xC2,
  RET        = 0xC3,
  MOV_MEM8_IMM = 0xC6,
  MOV_MEM_IMM = 0xC7,
  SHIFT8_1   = 0xD0,
  SHIFT_1    = 0xD1,
  SHIFT8_CL  = 0xD2,
  SHIFT_CL   = 0xD3,
  CALL32     = 0xE8,
  JMP32      = 0xE9,
  JMP8       = 0xEB,
  LOCK       = 0xF0,
  XMM_PREFIX = 0xF2,
  TEST_IMM8  = 0xF6,
  NOT_NEG8   = 0xF7,
  TEST_IMM32 = 0xF7,
  NOT_NEG32  = 0xF7,
  INC_DEC8   = 0xFE,
  INC_DEC    = 0xFF,
  PUSH_RM    = 0xFF,
  CALL_JMP_ABS = 0xFF,

  CVTSI2SD   = 0x0F2A,
  CVTSD2SI   = 0x0F2C,
  ROUNDSD    = 0x0F3A0B,

  CMOVO      = 0x0F40,
  CMOVNO     = 0x0F41,
  CMOVB      = 0x0F42,
  CMOVNAE    = 0x0F42,
  CMOVC      = 0x0F42,
  CMOVNB     = 0x0F43,
  CMOVAE     = 0x0F43,
  CMOVNC     = 0x0F43,
  CMOVZ      = 0x0F44,
  CMOVE      = 0x0F44,
  CMOVNZ     = 0x0F45,
  CMOVNE     = 0x0F45,
  CMOVBE     = 0x0F46,
  CMOVNA     = 0x0F46,
  CMOVNBE    = 0x0F47,
  CMOVA      = 0x0F47,
  CMOVS      = 0x0F48,
  CMOVNS     = 0x0F49,
  CMOVP      = 0x0F4A,
  CMOVPE     = 0x0F4A,
  CMOVNP     = 0x0F4B,
  CMOVPO     = 0x0F4B,
  CMOVL      = 0x0F4C,
  CMOVNGE    = 0x0F4C,
  CMOVNL     = 0x0F4D,
  CMOVGE     = 0x0F4D,
  CMOVLE     = 0x0F4E,
  CMOVNG     = 0x0F4E,
  CMOVNLE    = 0x0F4F,
  CMOVG      = 0x0F4F,

  MOVQ_TO_XMM   = 0x0F6E,
  MOVQ_FROM_XMM = 0x0F7E,
  JO         = 0x0F80,
  JNO        = 0x0F81,
  JB         = 0x0F82,
  JNAE       = 0x0F82,
  JC         = 0x0F82,
  JNB        = 0x0F83,
  JAE        = 0x0F83,
  JNC        = 0x0F83,
  JZ         = 0x0F84,
  JE         = 0x0F84,
  JNZ        = 0x0F85,
  JNE        = 0x0F85,
  JBE        = 0x0F86,
  JNA        = 0x0F86,
  JNBE       = 0x0F87,
  JA         = 0x0F87,
  JS         = 0x0F88,
  JNS        = 0x0F89,
  JP         = 0x0F8A,
  JPE        = 0x0F8A,
  JNP        = 0x0F8B,
  JPO        = 0x0F8B,
  JL         = 0x0F8C,
  JNGE       = 0x0F8C,
  JNL        = 0x0F8D,
  JGE        = 0x0F8D,
  JLE        = 0x0F8E,
  JNG        = 0x0F8E,
  JNLE       = 0x0F8F,
  JG         = 0x0F8F,
  SETO       = 0x0F90,
  SETNO      = 0x0F91,
  SETB       = 0x0F92,
  SETNAE     = 0x0F92,
  SETC       = 0x0F92,
  SETNB      = 0x0F93,
  SETAE      = 0x0F93,
  SETNC      = 0x0F93,
  SETZ       = 0x0F94,
  SETE       = 0x0F94,
  SETNZ      = 0x0F95,
  SETNE      = 0x0F95,
  SETBE      = 0x0F96,
  SETNA      = 0x0F96,
  SETNBE     = 0x0F97,
  SETA       = 0x0F97,
  SETS       = 0x0F98,
  SETNS      = 0x0F99,
  SETP       = 0x0F9A,
  SETPE      = 0x0F9A,
  SETNP      = 0x0F9B,
  SETPO      = 0x0F9B,
  SETL       = 0x0F9C,
  SETNGE     = 0x0F9C,
  SETNL      = 0x0F9D,
  SETGE      = 0x0F9D,
  SETLE      = 0x0F9E,
  SETNG      = 0x0F9E,
  SETNLE     = 0x0F9F,
  SETG       = 0x0F9F,
  IMUL       = 0x0FAF,
  MOVSD_LOAD  = 0x0F10,
  MOVSD_STORE = 0x0F11,
  ADDSD      = 0x0F58,
  MULSD      = 0x0F59,
  SUBSD      = 0x0F5C,
  MINSD      = 0x0F5D,
  DIVSD      = 0x0F5E,
  MAXSD      = 0x0F5F,
  MOVZX8     = 0x0FB6,
  MOVZX16    = 0x0FB7,
  CMPSD      = 0x0FC2,
};


enum Register {
  None = -1,

  RAX = 0,
  EAX = 0,
  AX = 0,
  AL = 0,
  XMM0 = 0,

  RCX = 1,
  ECX = 1,
  CX = 1,
  CL = 1,
  XMM1 = 1,

  RDX = 2,
  EDX = 2,
  DX = 2,
  DL = 2,
  XMM2 = 2,

  RBX = 3,
  EBX = 3,
  BX = 3,
  BL = 3,
  XMM3 = 3,

  RSP = 4,
  ESP = 4,
  SP = 4,
  AH = 4,
  XMM4 = 4,

  RBP = 5,
  EBP = 5,
  BP = 5,
  CH = 5,
  XMM5 = 5,

  RSI = 6,
  ESI = 6,
  SI = 6,
  DH = 6,
  XMM6 = 6,

  RDI = 7,
  EDI = 7,
  DI = 7,
  BH = 7,
  XMM7 = 7,

  R8 = 8,
  R8D = 8,
  R8W = 8,
  R8B = 8,
  XMM8 = 8,

  R9 = 9,
  R9D = 9,
  R9W = 9,
  R9B = 9,
  XMM9 = 9,

  R10 = 10,
  R10D = 10,
  R10W = 10,
  R10B = 10,
  XMM10 = 10,

  R11 = 11,
  R11D = 11,
  R11W = 11,
  R11B = 11,
  XMM11 = 11,

  R12 = 12,
  R12D = 12,
  R12W = 12,
  R12B = 12,
  XMM12 = 12,

  R13 = 13,
  R13D = 13,
  R13W = 13,
  R13B = 13,
  XMM13 = 13,

  R14 = 14,
  R14D = 14,
  R14W = 14,
  R14B = 14,
  XMM14 = 14,

  R15 = 15,
  R15D = 15,
  R15W = 15,
  R15B = 15,
  XMM15 = 15,

  Count = 16,

  RIP = 16,
  EIP = 16,
  IP = 16,

  SPL = 17,
  BPL = 18,
  SIL = 19,
  DIL = 20,
};

enum OperandSize {
  Unknown = -2,
  Automatic = -1,
  Byte = 0,
  Word = 1,
  DoubleWord = 2,
  QuadWord = 3,
  SinglePrecision = 4,
  DoublePrecision = 5,
  QuadWordXMM = 6,
};

const char* name_for_register(Register r,
    OperandSize size = OperandSize::QuadWord);

Register byte_register_for_register(Register r);

struct MemoryReference {
  Register base_register;
  Register index_register;
  int8_t field_size; // if 0, this is a register reference (not memory)
  int64_t offset;

  MemoryReference();
  MemoryReference(Register base_register, int64_t offset,
      Register index_register = Register::None, uint8_t field_size = 1);
  explicit MemoryReference(Register base_register);

  operator Register() const;

  bool operator==(const MemoryReference& other) const;
  bool operator!=(const MemoryReference& other) const;

  std::string str(OperandSize operand_size) const;
};

// shortcuts for all registers
extern MemoryReference rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15;
extern MemoryReference eax, ecx, edx, ebx, esp, ebp, esi, edi, r8d, r9d, r10d, r11d, r12d, r13d, r14d, r15d;
extern MemoryReference ax, cx, dx, bx, sp, bp, si, di, r8w, r9w, r10w, r11w, r12w, r13w, r14w, r15w;
extern MemoryReference al, cl, dl, bl, ah, ch, dh, bh, r8b, r9b, r10b, r11b, r12b, r13b, r14b, r15b, spl, bpl, sil, dil;
extern MemoryReference xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;

class AMD64Assembler {
public:
  AMD64Assembler() = default;
  AMD64Assembler(const AMD64Assembler&) = delete;
  AMD64Assembler(AMD64Assembler&&) = delete;
  AMD64Assembler& operator=(const AMD64Assembler&) = delete;
  AMD64Assembler& operator=(AMD64Assembler&&) = delete;
  ~AMD64Assembler() = default;

  // assemble the generated code (from previous calls to write_X functions) into
  // an executable binary string. returns the assembled data and a set of patch
  // offsets, which must be passed to CodeBuffer::append or
  // CodeBuffer::overwrite. of label_offsets is given, returns the label offsets
  // relative to the start of the assembled code. if base_address is nonzero,
  // the assembler uses it as a hint to convert absolute jumps into relative
  // jumps when possible. if autodefine_labels is true, doesn't fail on
  // undefined labels - instead, just leaves them as unpatched offsets. it's a
  // bad idea to execute any code assembled with autodefine_labels = true; it
  // should be used only for debugging.
  std::string assemble(std::unordered_set<size_t>& patch_offsets,
      std::multimap<size_t, std::string>* label_offsets = NULL,
      int64_t base_address = 0, bool autodefine_labels = false);

  // disassembles the given binary data into a (multi-line) human-readable
  // string.
  static std::string disassemble(const std::string& data, size_t addr = 0,
      const std::multimap<size_t, std::string>* label_offsets = NULL);
  static std::string disassemble(const void* vdata, size_t size, size_t addr = 0,
      const std::multimap<size_t, std::string>* label_offsets = NULL);

  // deletes the stored results of all previous write_X calls
  void reset();

  // don't do this unless you really know what you're doing
  void write_raw(const std::string& data);
  void write_raw(const void* data, size_t size);

  // label support
  void write_label(const std::string& name);
  void write_label_address(const std::string& name);

  // interrupt opcodes
  void write_int(uint8_t num);
  void write_syscall();

  // stack opcodes
  void write_push(Register r);
  void write_push(int64_t value);
  void write_push(const MemoryReference& mem);
  void write_pop(Register r);
  void write_pop(const MemoryReference& mem);

  // move opcodes
  void write_lea(Register r, const MemoryReference& mem);
  void write_mov(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_mov(Register reg, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_mov(Register reg, const std::string& label_name);
  void write_mov(const MemoryReference& mem, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_xchg(Register r, const MemoryReference& mem,
      OperandSize size = OperandSize::QuadWord);
  void write_movzx8(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_movzx16(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);

  // conditional move opcodes
  void write_cmovo(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovno(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovb(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnae(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovc(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnb(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovae(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnc(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovz(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmove(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnz(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovne(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovbe(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovna(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnbe(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmova(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovs(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovns(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovp(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovpe(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnp(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovpo(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovl(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnge(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnl(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovge(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovle(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovng(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovnle(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmovg(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);

  // floating-point stuff
  void write_movq_to_xmm(Register reg, const MemoryReference& from);
  void write_movq_from_xmm(const MemoryReference& from, Register reg);
  void write_movsd(const MemoryReference& to, const MemoryReference& from);
  void write_addsd(Register to, const MemoryReference& from);
  void write_subsd(Register to, const MemoryReference& from);
  void write_mulsd(Register to, const MemoryReference& from);
  void write_divsd(Register to, const MemoryReference& from);
  void write_minsd(Register to, const MemoryReference& from);
  void write_maxsd(Register to, const MemoryReference& from);
  void write_cmpsd(Register to, const MemoryReference& from, uint8_t which);
  void write_cmpeqsd(Register to, const MemoryReference& from);
  void write_cmpltsd(Register to, const MemoryReference& from);
  void write_cmplesd(Register to, const MemoryReference& from);
  void write_cmpunordsd(Register to, const MemoryReference& from);
  void write_cmpneqsd(Register to, const MemoryReference& from);
  void write_cmpnltsd(Register to, const MemoryReference& from);
  void write_cmpnlesd(Register to, const MemoryReference& from);
  void write_cmpordsd(Register to, const MemoryReference& from);
  void write_roundsd(Register to, const MemoryReference& from, uint8_t mode);
  void write_cvtsi2sd(Register to, const MemoryReference& from);
  void write_cvtsd2si(Register to, Register from);

  // control flow opcodes
  void write_nop();
  void write_jmp(const std::string& label_name);
  void write_jmp(const MemoryReference& mem);
  void write_jmp_abs(const void* addr);
  void write_call(const std::string& label_name);
  void write_call(const MemoryReference& mem);
  void write_call_abs(const void* addr);
  void write_ret(uint16_t stack_bytes = 0);
  void write_jo(const std::string& label_name);
  void write_jno(const std::string& label_name);
  void write_jb(const std::string& label_name);
  void write_jnae(const std::string& label_name);
  void write_jc(const std::string& label_name);
  void write_jnb(const std::string& label_name);
  void write_jae(const std::string& label_name);
  void write_jnc(const std::string& label_name);
  void write_jz(const std::string& label_name);
  void write_je(const std::string& label_name);
  void write_jnz(const std::string& label_name);
  void write_jne(const std::string& label_name);
  void write_jbe(const std::string& label_name);
  void write_jna(const std::string& label_name);
  void write_jnbe(const std::string& label_name);
  void write_ja(const std::string& label_name);
  void write_js(const std::string& label_name);
  void write_jns(const std::string& label_name);
  void write_jp(const std::string& label_name);
  void write_jpe(const std::string& label_name);
  void write_jnp(const std::string& label_name);
  void write_jpo(const std::string& label_name);
  void write_jl(const std::string& label_name);
  void write_jnge(const std::string& label_name);
  void write_jnl(const std::string& label_name);
  void write_jge(const std::string& label_name);
  void write_jle(const std::string& label_name);
  void write_jng(const std::string& label_name);
  void write_jnle(const std::string& label_name);
  void write_jg(const std::string& label_name);

  // math opcodes
  void write_add(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_add(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_or(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_or(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_adc(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_adc(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_sbb(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_sbb(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_and(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_and(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_sub(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_sub(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_xor(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_xor(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);

  void write_rol(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_ror(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_rcl(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_rcr(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_shl(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_shr(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_sar(const MemoryReference& to, uint8_t bits,
      OperandSize size = OperandSize::QuadWord);
  void write_rol_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_ror_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_rcl_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_rcr_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_shl_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_shr_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);
  void write_sar_cl(const MemoryReference& to,
      OperandSize size = OperandSize::QuadWord);

  void write_not(const MemoryReference& target,
      OperandSize size = OperandSize::QuadWord);
  void write_neg(const MemoryReference& target,
      OperandSize size = OperandSize::QuadWord);
  void write_inc(const MemoryReference& target,
      OperandSize size = OperandSize::QuadWord);
  void write_dec(const MemoryReference& target,
      OperandSize size = OperandSize::QuadWord);

  void write_imul(Register to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_mul(const MemoryReference& mem,
      OperandSize size = OperandSize::QuadWord);
  void write_imul(const MemoryReference& mem,
      OperandSize size = OperandSize::QuadWord);
  void write_div(const MemoryReference& mem,
      OperandSize size = OperandSize::QuadWord);
  void write_idiv(const MemoryReference& mem,
      OperandSize size = OperandSize::QuadWord);

  // comparison opcodes
  void write_cmp(const MemoryReference& to, const MemoryReference& from,
      OperandSize size = OperandSize::QuadWord);
  void write_cmp(const MemoryReference& to, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_test(const MemoryReference& a, const MemoryReference& b,
      OperandSize size = OperandSize::QuadWord);
  void write_test(const MemoryReference& a, int64_t value,
      OperandSize size = OperandSize::QuadWord);
  void write_seto(const MemoryReference& target);
  void write_setno(const MemoryReference& target);
  void write_setb(const MemoryReference& target);
  void write_setnae(const MemoryReference& target);
  void write_setc(const MemoryReference& target);
  void write_setnb(const MemoryReference& target);
  void write_setae(const MemoryReference& target);
  void write_setnc(const MemoryReference& target);
  void write_setz(const MemoryReference& target);
  void write_sete(const MemoryReference& target);
  void write_setnz(const MemoryReference& target);
  void write_setne(const MemoryReference& target);
  void write_setbe(const MemoryReference& target);
  void write_setna(const MemoryReference& target);
  void write_setnbe(const MemoryReference& target);
  void write_seta(const MemoryReference& target);
  void write_sets(const MemoryReference& target);
  void write_setns(const MemoryReference& target);
  void write_setp(const MemoryReference& target);
  void write_setpe(const MemoryReference& target);
  void write_setnp(const MemoryReference& target);
  void write_setpo(const MemoryReference& target);
  void write_setl(const MemoryReference& target);
  void write_setnge(const MemoryReference& target);
  void write_setnl(const MemoryReference& target);
  void write_setge(const MemoryReference& target);
  void write_setle(const MemoryReference& target);
  void write_setng(const MemoryReference& target);
  void write_setnle(const MemoryReference& target);
  void write_setg(const MemoryReference& target);

  // other stuff
  void write_lock();

private:
  static std::string generate_jmp(Operation op8, Operation op32,
      int64_t opcode_address, int64_t target_address, OperandSize* offset_size = NULL);
  static std::string generate_absolute_jmp_position_independent(Operation op8,
      Operation op32, int64_t target_address);
  static std::string generate_rm(Operation op, const MemoryReference& mem,
      Register reg, OperandSize size, uint32_t extra_prefixes = 0,
      bool skip_64bit_prefix = false);
  static std::string generate_rm(Operation op, const MemoryReference& mem,
      uint8_t z, OperandSize size, uint32_t extra_prefixes = 0,
      bool skip_64bit_prefix = false);
  void write_rm(Operation op, const MemoryReference& mem, Register reg,
      OperandSize size, uint32_t extra_prefixes = 0,
      bool skip_64bit_prefix = false);
  void write_rm(Operation op, const MemoryReference& mem, uint8_t z,
      OperandSize size, uint32_t extra_prefixes = 0,
      bool skip_64bit_prefix = false);
  static Operation load_store_oper_for_args(Operation op,
      const MemoryReference& to, const MemoryReference& from, OperandSize size);
  void write_load_store(Operation base_op, const MemoryReference& to,
      const MemoryReference& from, OperandSize size);
  void write_jcc(Operation op8, Operation op, const std::string& label_name);
  void write_imm_math(Operation math_op, const MemoryReference& to,
      int64_t value, OperandSize size);
  void write_shift(uint8_t which, const MemoryReference& mem, uint8_t bits,
      OperandSize size);
  void write_test_not_neg_imm(const MemoryReference& a, int64_t value,
      uint8_t z, OperandSize size);
  void write_mov_rm(const MemoryReference& mem, int64_t value, OperandSize size);

  void write(const std::string& opcode);

  struct Patch {
    size_t where;
    uint8_t size; // 1, 4, or 8
    bool absolute;

    Patch(size_t where, uint8_t size, bool absolute);

    std::string str() const;
  };

  struct StreamItem {
    std::string data; // for jump/call, label name; for data, opcode data

    // jump/call opcodes. if both are NOP, then this is a data item
    Operation op8;
    Operation op32;
    int64_t absolute_target;

    std::string patch_label_name; // blank for no patch
    Patch patch; // relative to start of data string

    StreamItem(const std::string& data);
    StreamItem(const std::string& data, const std::string& patch_label_name,
        size_t where, uint8_t size, bool absolute);
    StreamItem(const std::string& data, Operation op8, Operation op32,
        int64_t absolute_target = 0);

    std::string str() const;

    bool is_jump_call() const;
  };
  std::deque<StreamItem> stream;

  struct Label {
    std::string name;
    size_t stream_location;
    size_t byte_location;
    std::deque<Patch> patches;

    Label(const std::string& name, size_t stream_location);
    Label(const Label&) = delete;
    Label(Label&&) = default;

    std::string str() const;
  };
  std::deque<Label> labels;
  std::unordered_map<std::string, Label*> name_to_label;

  static std::string disassemble_rm(const uint8_t* data, size_t size,
      size_t& offset, const char* opcode_name, bool is_load,
      const char** op_name_table, bool ext, bool reg_ext, bool base_ext,
      bool index_ext, OperandSize reg_operand_size,
      OperandSize mem_operand_size = OperandSize::Automatic);
  static std::string disassemble_jmp(const uint8_t* data, size_t size,
      size_t& offset, uint64_t addr, const char* opcode_name, bool is_32bit,
      std::multimap<size_t, std::string>& addr_to_label, uint64_t& next_label);
  static std::string disassemble_imm(const uint8_t* data, size_t size,
      size_t& offset, OperandSize operand_size, bool allow_64bit = false);
};
