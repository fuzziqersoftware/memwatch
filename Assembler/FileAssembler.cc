#include "FileAssembler.hh"

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <string>
#include <set>

#include "AMD64Assembler.hh"

using namespace std;



struct RegisterDefinition {
  Register reg;
  OperandSize size;
};

static const unordered_map<string, RegisterDefinition> name_to_register_def({
  {"rax", {Register::RAX, OperandSize::QuadWord}},
  {"rcx", {Register::RCX, OperandSize::QuadWord}},
  {"rdx", {Register::RDX, OperandSize::QuadWord}},
  {"rbx", {Register::RBX, OperandSize::QuadWord}},
  {"rsp", {Register::RSP, OperandSize::QuadWord}},
  {"rbp", {Register::RBP, OperandSize::QuadWord}},
  {"rsi", {Register::RSI, OperandSize::QuadWord}},
  {"rdi", {Register::RDI, OperandSize::QuadWord}},
  {"r8", {Register::R8, OperandSize::QuadWord}},
  {"r9", {Register::R9, OperandSize::QuadWord}},
  {"r10", {Register::R10, OperandSize::QuadWord}},
  {"r11", {Register::R11, OperandSize::QuadWord}},
  {"r12", {Register::R12, OperandSize::QuadWord}},
  {"r13", {Register::R13, OperandSize::QuadWord}},
  {"r14", {Register::R14, OperandSize::QuadWord}},
  {"r15", {Register::R15, OperandSize::QuadWord}},
  {"eax", {Register::EAX, OperandSize::DoubleWord}},
  {"ecx", {Register::ECX, OperandSize::DoubleWord}},
  {"edx", {Register::EDX, OperandSize::DoubleWord}},
  {"ebx", {Register::EBX, OperandSize::DoubleWord}},
  {"esp", {Register::ESP, OperandSize::DoubleWord}},
  {"ebp", {Register::EBP, OperandSize::DoubleWord}},
  {"esi", {Register::ESI, OperandSize::DoubleWord}},
  {"edi", {Register::EDI, OperandSize::DoubleWord}},
  {"r8d", {Register::R8D, OperandSize::DoubleWord}},
  {"r9d", {Register::R9D, OperandSize::DoubleWord}},
  {"r10d", {Register::R10D, OperandSize::DoubleWord}},
  {"r11d", {Register::R11D, OperandSize::DoubleWord}},
  {"r12d", {Register::R12D, OperandSize::DoubleWord}},
  {"r13d", {Register::R13D, OperandSize::DoubleWord}},
  {"r14d", {Register::R14D, OperandSize::DoubleWord}},
  {"r15d", {Register::R15D, OperandSize::DoubleWord}},
  {"ax", {Register::AX, OperandSize::Word}},
  {"cx", {Register::CX, OperandSize::Word}},
  {"dx", {Register::DX, OperandSize::Word}},
  {"bx", {Register::BX, OperandSize::Word}},
  {"sp", {Register::SP, OperandSize::Word}},
  {"bp", {Register::BP, OperandSize::Word}},
  {"si", {Register::SI, OperandSize::Word}},
  {"di", {Register::DI, OperandSize::Word}},
  {"r8w", {Register::R8W, OperandSize::Word}},
  {"r9w", {Register::R9W, OperandSize::Word}},
  {"r10w", {Register::R10W, OperandSize::Word}},
  {"r11w", {Register::R11W, OperandSize::Word}},
  {"r12w", {Register::R12W, OperandSize::Word}},
  {"r13w", {Register::R13W, OperandSize::Word}},
  {"r14w", {Register::R14W, OperandSize::Word}},
  {"r15w", {Register::R15W, OperandSize::Word}},
  {"al", {Register::AL, OperandSize::Byte}},
  {"cl", {Register::CL, OperandSize::Byte}},
  {"dl", {Register::DL, OperandSize::Byte}},
  {"bl", {Register::BL, OperandSize::Byte}},
  {"ah", {Register::AH, OperandSize::Byte}},
  {"ch", {Register::CH, OperandSize::Byte}},
  {"dh", {Register::DH, OperandSize::Byte}},
  {"bh", {Register::BH, OperandSize::Byte}},
  {"r8b", {Register::R8B, OperandSize::Byte}},
  {"r9b", {Register::R9B, OperandSize::Byte}},
  {"r10b", {Register::R10B, OperandSize::Byte}},
  {"r11b", {Register::R11B, OperandSize::Byte}},
  {"r12b", {Register::R12B, OperandSize::Byte}},
  {"r13b", {Register::R13B, OperandSize::Byte}},
  {"r14b", {Register::R14B, OperandSize::Byte}},
  {"r15b", {Register::R15B, OperandSize::Byte}},
  {"spl", {Register::SPL, OperandSize::Byte}},
  {"bpl", {Register::BPL, OperandSize::Byte}},
  {"sil", {Register::SIL, OperandSize::Byte}},
  {"dil", {Register::DIL, OperandSize::Byte}},
  {"xmm0", {Register::XMM0, OperandSize::QuadWordXMM}},
  {"xmm1", {Register::XMM1, OperandSize::QuadWordXMM}},
  {"xmm2", {Register::XMM2, OperandSize::QuadWordXMM}},
  {"xmm3", {Register::XMM3, OperandSize::QuadWordXMM}},
  {"xmm4", {Register::XMM4, OperandSize::QuadWordXMM}},
  {"xmm5", {Register::XMM5, OperandSize::QuadWordXMM}},
  {"xmm6", {Register::XMM6, OperandSize::QuadWordXMM}},
  {"xmm7", {Register::XMM7, OperandSize::QuadWordXMM}},
  {"xmm8", {Register::XMM8, OperandSize::QuadWordXMM}},
  {"xmm9", {Register::XMM9, OperandSize::QuadWordXMM}},
  {"xmm10", {Register::XMM10, OperandSize::QuadWordXMM}},
  {"xmm11", {Register::XMM11, OperandSize::QuadWordXMM}},
  {"xmm12", {Register::XMM12, OperandSize::QuadWordXMM}},
  {"xmm13", {Register::XMM13, OperandSize::QuadWordXMM}},
  {"xmm14", {Register::XMM14, OperandSize::QuadWordXMM}},
  {"xmm15", {Register::XMM15, OperandSize::QuadWordXMM}},
});



struct InstructionArgument {
  enum class Type {
    MemoryReference = 0,
    LabelReference,
    Constant,
  };
  Type type;

  MemoryReference reference;
  string label_name;
  int64_t value;
};

struct InstructionAssemblyDefinition {
  vector<InstructionArgument::Type> arg_types;
  set<OperandSize> sizes;
  void (*assemble)(AMD64Assembler& as, const vector<InstructionArgument>& args, OperandSize size);

  bool typecheck(const vector<InstructionArgument>& args, OperandSize size) const {
    if (!this->sizes.count(size)) {
      return false;
    }

    if (args.size() != this->arg_types.size()) {
      return false;
    }

    for (size_t x = 0; x < args.size(); x++) {
      if (args[x].type != this->arg_types[x]) {
        return false;
      }
    }

    return true;
  }
};

using ArgType = InstructionArgument::Type;

#define handler_t +[](AMD64Assembler& as, const vector<InstructionArgument>& args, OperandSize size)

static const set<OperandSize> no_size({OperandSize::Unknown});
static const set<OperandSize> all_sizes({OperandSize::Byte, OperandSize::Word, OperandSize::DoubleWord, OperandSize::QuadWord});
static const set<OperandSize> byte_size({OperandSize::Byte});
static const set<OperandSize> optional_byte_size({OperandSize::Unknown, OperandSize::Byte});
static const set<OperandSize> qword_size({OperandSize::QuadWord});
static const set<OperandSize> optional_qword_size({OperandSize::Unknown, OperandSize::QuadWord});

unordered_multimap<string, InstructionAssemblyDefinition> assembly_definitions({
  {"int", {{ArgType::Constant}, no_size, handler_t {
    as.write_int(args[0].value);
  }}},
  {"syscall", {{}, no_size, handler_t {
    as.write_syscall();
  }}},

  {"push", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_push(args[0].reference);
  }}},
  {"push", {{ArgType::Constant}, no_size, handler_t {
    as.write_push(args[0].value);
  }}},
  {"pop", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_pop(args[0].reference);
  }}},

  {"lea", {{ArgType::MemoryReference, ArgType::MemoryReference}, qword_size, handler_t {
    if (args[0].reference.field_size != 0) {
      throw invalid_argument("lea first operand must be a register");
    }
    as.write_lea(args[0].reference.base_register, args[1].reference);
  }}},

  {"mov", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_mov(args[0].reference, args[1].value, size);
  }}},
  {"mov", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_mov(args[0].reference, args[1].reference, size);
  }}},
  {"xchg", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_xchg(args[0].reference, args[1].reference, size);
  }}},

  {"nop", {{}, no_size, handler_t {
    as.write_nop();
  }}},

  {"jmp", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jmp(args[0].label_name);
  }}},
  {"jmp", {{ArgType::MemoryReference}, optional_qword_size, handler_t {
    as.write_jmp(args[0].reference);
  }}},
  {"call", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_call(args[0].label_name);
  }}},
  {"call", {{ArgType::MemoryReference}, optional_qword_size, handler_t {
    as.write_call(args[0].reference);
  }}},
  {"ret", {{}, no_size, handler_t {
    as.write_ret();
  }}},
  {"ret", {{ArgType::Constant}, no_size, handler_t {
    as.write_ret(args[0].value);
  }}},

  {"jo", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jo(args[0].label_name);
  }}},
  {"jno", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jno(args[0].label_name);
  }}},
  {"jb", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jb(args[0].label_name);
  }}},
  {"jnae", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnae(args[0].label_name);
  }}},
  {"jc", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jc(args[0].label_name);
  }}},
  {"jnb", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnb(args[0].label_name);
  }}},
  {"jae", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jae(args[0].label_name);
  }}},
  {"jnc", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnc(args[0].label_name);
  }}},
  {"jz", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jz(args[0].label_name);
  }}},
  {"je", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_je(args[0].label_name);
  }}},
  {"jnz", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnz(args[0].label_name);
  }}},
  {"jne", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jne(args[0].label_name);
  }}},
  {"jbe", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jbe(args[0].label_name);
  }}},
  {"jna", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jna(args[0].label_name);
  }}},
  {"jnbe", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnbe(args[0].label_name);
  }}},
  {"ja", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_ja(args[0].label_name);
  }}},
  {"js", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_js(args[0].label_name);
  }}},
  {"jns", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jns(args[0].label_name);
  }}},
  {"jp", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jp(args[0].label_name);
  }}},
  {"jpe", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jpe(args[0].label_name);
  }}},
  {"jnp", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnp(args[0].label_name);
  }}},
  {"jpo", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jpo(args[0].label_name);
  }}},
  {"jl", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jl(args[0].label_name);
  }}},
  {"jnge", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnge(args[0].label_name);
  }}},
  {"jnl", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnl(args[0].label_name);
  }}},
  {"jge", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jge(args[0].label_name);
  }}},
  {"jle", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jle(args[0].label_name);
  }}},
  {"jng", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jng(args[0].label_name);
  }}},
  {"jnle", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jnle(args[0].label_name);
  }}},
  {"jg", {{ArgType::LabelReference}, no_size, handler_t {
    as.write_jg(args[0].label_name);
  }}},

  // math opcodes
  {"add", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_add(args[0].reference, args[1].reference, size);
  }}},
  {"add", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_add(args[0].reference, args[1].value, size);
  }}},
  {"or", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_or(args[0].reference, args[1].reference, size);
  }}},
  {"or", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_or(args[0].reference, args[1].value, size);
  }}},
  {"adc", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_adc(args[0].reference, args[1].reference, size);
  }}},
  {"adc", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_adc(args[0].reference, args[1].value, size);
  }}},
  {"sbb", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_sbb(args[0].reference, args[1].reference, size);
  }}},
  {"sbb", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_sbb(args[0].reference, args[1].value, size);
  }}},
  {"and", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_and(args[0].reference, args[1].reference, size);
  }}},
  {"and", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_and(args[0].reference, args[1].value, size);
  }}},
  {"sub", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_sub(args[0].reference, args[1].reference, size);
  }}},
  {"sub", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_sub(args[0].reference, args[1].value, size);
  }}},
  {"xor", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_xor(args[0].reference, args[1].reference, size);
  }}},
  {"xor", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_xor(args[0].reference, args[1].value, size);
  }}},
  {"rol", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_rol(args[0].reference, args[1].value, size);
  }}},
  {"ror", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_ror(args[0].reference, args[1].value, size);
  }}},
  {"rcl", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_rcl(args[0].reference, args[1].value, size);
  }}},
  {"rcr", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_rcr(args[0].reference, args[1].value, size);
  }}},
  {"shl", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_shl(args[0].reference, args[1].value, size);
  }}},
  {"shr", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_shr(args[0].reference, args[1].value, size);
  }}},
  {"sar", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_sar(args[0].reference, args[1].value, size);
  }}},

  {"not", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_not(args[0].reference, size);
  }}},
  {"neg", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_neg(args[0].reference, size);
  }}},
  {"inc", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_inc(args[0].reference, size);
  }}},
  {"dec", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_dec(args[0].reference, size);
  }}},

  {"mul", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_mul(args[0].reference, size);
  }}},
  {"imul", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_imul(args[0].reference, size);
  }}},
  {"div", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_div(args[0].reference, size);
  }}},
  {"idiv", {{ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_idiv(args[0].reference, size);
  }}},

  {"imul", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    if (args[0].reference.field_size != 0) {
      throw invalid_argument("lea first operand must be a register");
    }
    as.write_imul(args[0].reference.base_register, args[1].reference, size);
  }}},

  // comparison opcodes
  {"cmp", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_cmp(args[0].reference, args[1].reference, size);
  }}},
  {"cmp", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_cmp(args[0].reference, args[1].value, size);
  }}},
  {"test", {{ArgType::MemoryReference, ArgType::MemoryReference}, all_sizes, handler_t {
    as.write_test(args[0].reference, args[1].reference, size);
  }}},
  {"test", {{ArgType::MemoryReference, ArgType::Constant}, all_sizes, handler_t {
    as.write_test(args[0].reference, args[1].value, size);
  }}},

  {"seto", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_seto(args[0].reference);
  }}},
  {"setno", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setno(args[0].reference);
  }}},
  {"setb", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setb(args[0].reference);
  }}},
  {"setnae", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnae(args[0].reference);
  }}},
  {"setc", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setc(args[0].reference);
  }}},
  {"setnb", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnb(args[0].reference);
  }}},
  {"setae", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setae(args[0].reference);
  }}},
  {"setnc", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnc(args[0].reference);
  }}},
  {"setz", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setz(args[0].reference);
  }}},
  {"sete", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_sete(args[0].reference);
  }}},
  {"setnz", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnz(args[0].reference);
  }}},
  {"setne", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setne(args[0].reference);
  }}},
  {"setbe", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setbe(args[0].reference);
  }}},
  {"setna", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setna(args[0].reference);
  }}},
  {"setnbe", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnbe(args[0].reference);
  }}},
  {"seta", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_seta(args[0].reference);
  }}},
  {"sets", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_sets(args[0].reference);
  }}},
  {"setns", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setns(args[0].reference);
  }}},
  {"setp", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setp(args[0].reference);
  }}},
  {"setpe", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setpe(args[0].reference);
  }}},
  {"setnp", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnp(args[0].reference);
  }}},
  {"setpo", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setpo(args[0].reference);
  }}},
  {"setl", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setl(args[0].reference);
  }}},
  {"setnge", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnge(args[0].reference);
  }}},
  {"setnl", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnl(args[0].reference);
  }}},
  {"setge", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setge(args[0].reference);
  }}},
  {"setle", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setle(args[0].reference);
  }}},
  {"setng", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setng(args[0].reference);
  }}},
  {"setnle", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setnle(args[0].reference);
  }}},
  {"setg", {{ArgType::MemoryReference}, optional_byte_size, handler_t {
    as.write_setg(args[0].reference);
  }}},

  {"lock", {{}, no_size, handler_t {
    as.write_lock();
  }}}
});

#undef handler_t



static void remove_empty(vector<string>& t) {
  for (size_t x = 0; x < t.size();) {
    if (t[x].empty()) {
      t.erase(t.begin() + x);
    } else {
      x++;
    }
  }
}

static string strip(const string& s) {
  size_t start_offset = s.find_first_not_of(" \t\r\n");
  if (start_offset == string::npos) {
    return "";
  }
  size_t end_offset = s.find_last_not_of(" \t\r\n");
  return s.substr(start_offset, end_offset - start_offset + 1);
}

static int64_t stoll_or_stoull(const string& s, size_t* pos, size_t base) {
  try {
    return stoll(s, pos, base);
  } catch (const out_of_range&) {
    return stoull(s, pos, base);
  }
}

static void assemble_line(AMD64Assembler& as, const string& raw_line) {
  string line = raw_line;

  // get rid of comments and excess whitespace
  {
    size_t comment_pos = line.find("//");
    if (comment_pos != string::npos) {
      line = line.substr(0, comment_pos);
    }
    comment_pos = line.find_first_of(";#");
    if (comment_pos != string::npos) {
      line = line.substr(0, comment_pos);
    }
  }
  line = strip(line);

  // if the line ends with a :, it's a label
  if (ends_with(line, ":")) {
    as.write_label(line.substr(0, line.size() - 1));
    return;
  }

  // if the line starts with a ., it's a directive
  if (starts_with(line, ".")) {
    if (!line.compare(0, 7, ".global", 7) || !line.compare(0, 6, ".globl", 6)) {
      // ignore these; all symbols are exported to the caller
    } else if (!line.compare(0, 13, ".intel_syntax", 13)) {
      // ignore this; we always use intel syntax
    } else {
      throw invalid_argument("unknown directive: " + line);
    }
    return;
  }

  // if we get here, then the line is an instruction

  // split the arguments. the first token will include the instruction too
  auto arg_tokens = split(line, ',');
  if (arg_tokens.empty()) {
    return;
  }

  // get the instruction name from the first token
  string instruction_name;
  {
    auto tokens2 = split(arg_tokens[0], ' ');
    remove_empty(tokens2);
    if (tokens2.empty()) {
      throw invalid_argument("no instruction on line: " + raw_line);
    }
    instruction_name = tokens2[0];
    tokens2.erase(tokens2.begin());
    arg_tokens[0] = join(tokens2, " ");
  }
  remove_empty(arg_tokens);

  // now parse the arguments
  vector<InstructionArgument> args;
  OperandSize size = OperandSize::Unknown;
  for (auto& arg_token : arg_tokens) {
    arg_token = strip(arg_token);
    if (arg_token.empty()) {
      throw invalid_argument("invalid empty argument");
    }

    args.emplace_back();
    auto& arg = args.back();

    // check for register references
    try {
      const auto& def = name_to_register_def.at(arg_token);
      arg.reference = MemoryReference(def.reg);
      if (size == OperandSize::Unknown) {
        size = def.size;
      } else if (size != def.size) {
        throw invalid_argument("conflicting operand sizes");
      }

    } catch (const out_of_range&) {

      // check for memory references
      size_t bracket_offset = arg_token.find('[');
      if (bracket_offset != string::npos) {
        if (arg_token[arg_token.size() - 1] != ']') {
          throw invalid_argument("invalid memory reference");
        }

        // if there's a prefix specifying the operand size, parse it out
        if (bracket_offset != 0) {
          string prefix_str = strip(arg_token.substr(0, bracket_offset));
          if (ends_with(prefix_str, " ptr")) {
            prefix_str = strip(prefix_str.substr(0, prefix_str.size() - 4));
          }

          OperandSize prefix_size;
          if (prefix_str == "byte") {
            prefix_size = OperandSize::Byte;
          } else if (prefix_str == "word") {
            prefix_size = OperandSize::Word;
          } else if (prefix_str == "dword") {
            prefix_size = OperandSize::DoubleWord;
          } else if (prefix_str == "qword") {
            prefix_size = OperandSize::QuadWord;
          } else {
            throw invalid_argument("invalid operand size prefix");
          }

          if (size == OperandSize::Unknown) {
            size = prefix_size;
          } else if (size != prefix_size) {
            throw invalid_argument("conflicting operand sizes");
          }
        }

        // memory references always have field_size set to something nonzero,
        // even if the index register is Register::None
        arg.reference.field_size = 1;

        // remove [] and spaces from the expression
        string expr;
        for (size_t x = bracket_offset + 1; x < arg_token.size() - 1; x++) {
          if (arg_token[x] == ' ') {
            continue;
          }
          expr.push_back(arg_token[x]);
        }
        if (expr.empty()) {
          throw invalid_argument("empty memory reference expression");
        }

        vector<string> expr_tokens({""});
        vector<bool> expr_tokens_negative({false});
        for (char ch : expr) {
          if (ch == ' ') {
            continue;
          }
          if (ch == '-') {
            // check for leading - sign
            if ((expr_tokens.size() == 1) && (expr_tokens.back().size() == 0)) {
              expr_tokens_negative[0] = true;
            } else {
              expr_tokens.emplace_back();
              expr_tokens_negative.emplace_back(true);
            }
          } else if (ch == '+') {
            expr_tokens.emplace_back();
            expr_tokens_negative.emplace_back(false);
          } else {
            expr_tokens.back().push_back(ch);
          }
        }

        if (expr_tokens.size() > 3) {
          throw invalid_argument("invalid memory reference expression");
        }

        for (size_t x = 0; x < expr_tokens.size(); x++) {
          const string& expr_token = expr_tokens[x];
          bool expr_token_negative = expr_tokens_negative[x];

          // check if the token contains *; if so, make it the index reg
          size_t star_pos = expr_token.find('*');
          if (star_pos != string::npos) {
            if (expr_token_negative) {
              throw invalid_argument("index register references must be positive");
            }
            if (arg.reference.index_register != Register::None) {
              throw invalid_argument("memory reference expression contains multiple indexes");
            }

            // this token is either i*reg or reg*i
            string e1 = expr_token.substr(0, star_pos);
            string e2 = expr_token.substr(star_pos + 1);
            try {
              const auto& def = name_to_register_def.at(e1);
              arg.reference.index_register = def.reg;
              arg.reference.field_size = stoul(e2, NULL, 0);
            } catch (const out_of_range&) {
              try {
                const auto& def = name_to_register_def.at(e2);
                arg.reference.index_register = def.reg;
                arg.reference.field_size = stoul(e1, NULL, 0);
              } catch (const out_of_range&) {
                throw invalid_argument("neither side of index multiplication is a register");
              }
            }

          } else {
            // token doesn't contain *. it might be a normal register, a constant, or a label
            try {
              const auto& def = name_to_register_def.at(expr_token);
              if (expr_token_negative) {
                throw invalid_argument("register references must be positive");
              }
              if (arg.reference.base_register == Register::None) {
                arg.reference.base_register = def.reg;
              } else if (arg.reference.index_register == Register::None) {
                arg.reference.index_register = def.reg;
              } else {
                throw invalid_argument("too many register references in memory reference expression");
              }

            } catch (const out_of_range&) {
              // it must be a constant
              size_t converted_bytes;
              arg.reference.offset += stoll_or_stoull(expr_token, &converted_bytes, 0);
              if (converted_bytes != expr_token.size()) {
                throw invalid_argument("invalid token in memory reference expression");
              }
              if (expr_token_negative) {
                arg.reference.offset = -arg.reference.offset;
              }
            }
          }
        }

      } else {
        // check for constants
        try {
          size_t converted_bytes;
          arg.value = stoll_or_stoull(arg_token, &converted_bytes, 0);
          arg.type = InstructionArgument::Type::Constant;
          if (converted_bytes != arg_token.size()) {
            throw invalid_argument("incomplete constant");
          }

        } catch (const invalid_argument&) {
          arg.type = InstructionArgument::Type::LabelReference;
          arg.label_name = arg_token;
        }
      }
    }
  }

  // assemble the instruction
  auto range = assembly_definitions.equal_range(instruction_name);
  if (range.first == range.second) {
    throw invalid_argument("unknown instruction: " + instruction_name);
  }

  for (; range.first != range.second; range.first++) {
    const auto& def = range.first->second;

    if (!def.typecheck(args, size)) {
      continue;
    }

    def.assemble(as, args, size);
    return;
  }

  throw invalid_argument("no typechecks matched");
}



AssembledFile assemble_file(const string& data) {
  auto lines = split(data, '\n');

  // assemble it to stdout
  AssembledFile ret;
  AMD64Assembler as;
  for (size_t x = 0; x < lines.size(); x++) {
    try {
      assemble_line(as, lines[x]);
    } catch (const exception& e) {
      ret.errors.emplace_back(string_printf("error: line %zu: %s", x + 1, e.what()));
    }
  }

  try {
    ret.code = as.assemble(ret.patch_offsets, &ret.label_offsets);
  } catch (const exception& e) {
    ret.errors.emplace_back(string_printf("assembly error: %s", e.what()));
  }

  return ret;
}
