#include "AMD64Assembler.hh"

#include <inttypes.h>

#include <phosg/Strings.hh>
#include <string>

using namespace std;


const char* name_for_register(Register r, OperandSize size) {
  switch (size) {
    case OperandSize::Unknown:
      throw invalid_argument("unknown operand size");

    case OperandSize::Automatic:
      throw invalid_argument("unresolved operand size");

    case OperandSize::Byte:
      switch (r) {
        case Register::None:
          return "None";
        case Register::AL:
          return "al";
        case Register::CL:
          return "cl";
        case Register::DL:
          return "dl";
        case Register::BL:
          return "bl";
        case Register::AH:
          return "ah";
        case Register::CH:
          return "ch";
        case Register::DH:
          return "dh";
        case Register::BH:
          return "bh";
        case Register::R8B:
          return "r8b";
        case Register::R9B:
          return "r9b";
        case Register::R10B:
          return "r10b";
        case Register::R11B:
          return "r11b";
        case Register::R12B:
          return "r12b";
        case Register::R13B:
          return "r13b";
        case Register::R14B:
          return "r14b";
        case Register::R15B:
          return "r15b";
        case Register::SPL:
          return "spl";
        case Register::BPL:
          return "bpl";
        case Register::SIL:
          return "sil";
        case Register::DIL:
          return "dil";
        default:
          return "UNKNOWN8";
      }
    case OperandSize::Word:
      switch (r) {
        case Register::None:
          return "None";
        case Register::AX:
          return "ax";
        case Register::CX:
          return "cx";
        case Register::DX:
          return "dx";
        case Register::BX:
          return "bx";
        case Register::SP:
          return "sp";
        case Register::BP:
          return "bp";
        case Register::SI:
          return "si";
        case Register::DI:
          return "di";
        case Register::R8W:
          return "r8w";
        case Register::R9W:
          return "r9w";
        case Register::R10W:
          return "r10w";
        case Register::R11W:
          return "r11w";
        case Register::R12W:
          return "r12w";
        case Register::R13W:
          return "r13w";
        case Register::R14W:
          return "r14w";
        case Register::R15W:
          return "r15w";
        default:
          return "UNKNOWN16";
      }
    case OperandSize::DoubleWord:
      switch (r) {
        case Register::None:
          return "None";
        case Register::EAX:
          return "eax";
        case Register::ECX:
          return "ecx";
        case Register::EDX:
          return "edx";
        case Register::EBX:
          return "ebx";
        case Register::ESP:
          return "esp";
        case Register::EBP:
          return "ebp";
        case Register::ESI:
          return "esi";
        case Register::EDI:
          return "edi";
        case Register::R8D:
          return "r8d";
        case Register::R9D:
          return "r9d";
        case Register::R10D:
          return "r10d";
        case Register::R11D:
          return "r11d";
        case Register::R12D:
          return "r12d";
        case Register::R13D:
          return "r13d";
        case Register::R14D:
          return "r14d";
        case Register::R15D:
          return "r15d";
        default:
          return "UNKNOWN32";
      }
    case OperandSize::QuadWord:
      switch (r) {
        case Register::None:
          return "None";
        case Register::RAX:
          return "rax";
        case Register::RCX:
          return "rcx";
        case Register::RDX:
          return "rdx";
        case Register::RBX:
          return "rbx";
        case Register::RSP:
          return "rsp";
        case Register::RBP:
          return "rbp";
        case Register::RSI:
          return "rsi";
        case Register::RDI:
          return "rdi";
        case Register::R8:
          return "r8";
        case Register::R9:
          return "r9";
        case Register::R10:
          return "r10";
        case Register::R11:
          return "r11";
        case Register::R12:
          return "r12";
        case Register::R13:
          return "r13";
        case Register::R14:
          return "r14";
        case Register::R15:
          return "r15";
        default:
          return "UNKNOWN64";
      }

    case OperandSize::SinglePrecision:
    case OperandSize::DoublePrecision:
    case OperandSize::QuadWordXMM:
      switch (r) {
        case Register::XMM0:
          return "xmm0";
        case Register::XMM1:
          return "xmm1";
        case Register::XMM2:
          return "xmm2";
        case Register::XMM3:
          return "xmm3";
        case Register::XMM4:
          return "xmm4";
        case Register::XMM5:
          return "xmm5";
        case Register::XMM6:
          return "xmm6";
        case Register::XMM7:
          return "xmm7";
        case Register::XMM8:
          return "xmm8";
        case Register::XMM9:
          return "xmm9";
        case Register::XMM10:
          return "xmm10";
        case Register::XMM11:
          return "xmm11";
        case Register::XMM12:
          return "xmm12";
        case Register::XMM13:
          return "xmm13";
        case Register::XMM14:
          return "xmm14";
        case Register::XMM15:
          return "xmm15";
        default:
          return "UNKNOWNFLOAT";
      }
  }
  return "UNKNOWN";
}

Register byte_register_for_register(Register r) {
  switch (r) {
    case Register::None:
      return Register::None;
    case Register::RAX:
      return Register::AL;
    case Register::RCX:
      return Register::CL;
    case Register::RDX:
      return Register::DL;
    case Register::RBX:
      return Register::BL;
    case Register::RSP:
      return Register::SPL;
    case Register::RBP:
      return Register::BPL;
    case Register::RSI:
      return Register::SIL;
    case Register::RDI:
      return Register::DIL;
    case Register::R8:
      return Register::R8B;
    case Register::R9:
      return Register::R9B;
    case Register::R10:
      return Register::R10B;
    case Register::R11:
      return Register::R11B;
    case Register::R12:
      return Register::R12B;
    case Register::R13:
      return Register::R13B;
    case Register::R14:
      return Register::R14B;
    case Register::R15:
      return Register::R15B;
    default:
      return Register::None;
  }
}

static string data_for_opcode(uint32_t opcode) {
  string ret;
  if (opcode > 0xFFFFFF) {
    ret += (opcode >> 24) & 0xFF;
  }
  if (opcode > 0xFFFF) {
    ret += (opcode >> 16) & 0xFF;
  }
  if (opcode > 0xFF) {
    ret += (opcode >> 8) & 0xFF;
  }
  ret += opcode & 0xFF;
  return ret;
}



MemoryReference::MemoryReference() : base_register(Register::None),
    index_register(Register::None), field_size(0), offset(0) { }
MemoryReference::MemoryReference(Register base_register, int64_t offset,
    Register index_register, uint8_t field_size) : base_register(base_register),
    index_register(index_register), field_size(field_size), offset(offset) { }
MemoryReference::MemoryReference(Register base_register) :
    base_register(base_register), index_register(Register::None), field_size(0),
    offset(0) { }

MemoryReference::operator Register() const {
  if (this->offset || field_size) {
    throw invalid_argument("cannot convert nontrivial memory reference to reg");
  }
  return this->base_register;
}

bool MemoryReference::operator==(const MemoryReference& other) const {
  return (this->base_register == other.base_register) &&
         (this->index_register == other.index_register) &&
         (this->field_size == other.field_size) &&
         (this->offset == other.offset);
}

bool MemoryReference::operator!=(const MemoryReference& other) const {
  return !this->operator==(other);
}

string MemoryReference::str(OperandSize operand_size) const {
  if (!this->field_size) {
    return name_for_register(this->base_register, operand_size);
  }

  string ret = "[";
  if (this->base_register != Register::None) {
    ret += name_for_register(this->base_register, OperandSize::QuadWord);
  }
  if (this->index_register != Register::None) {
    if (ret.size() > 1) {
      ret += " + ";
    }
    if (this->field_size > 1) {
      ret += string_printf("%hhd * ", this->field_size);
    }
    ret += name_for_register(this->index_register, OperandSize::QuadWord);
  }
  if (this->offset) {
    if (ret.size() > 1) {
      if (offset < 0) {
        ret += " - ";
      } else {
        ret += " + ";
      }
    }
    ret += string_printf("0x%" PRIX64,
        (this->offset < 0) ? -this->offset : this->offset);
  }
  return ret + "]";
}

MemoryReference rax(Register::RAX);
MemoryReference rcx(Register::RCX);
MemoryReference rdx(Register::RDX);
MemoryReference rbx(Register::RBX);
MemoryReference rsp(Register::RSP);
MemoryReference rbp(Register::RBP);
MemoryReference rsi(Register::RSI);
MemoryReference rdi(Register::RDI);
MemoryReference r8(Register::R8);
MemoryReference r9(Register::R9);
MemoryReference r10(Register::R10);
MemoryReference r11(Register::R11);
MemoryReference r12(Register::R12);
MemoryReference r13(Register::R13);
MemoryReference r14(Register::R14);
MemoryReference r15(Register::R15);
MemoryReference eax(Register::EAX);
MemoryReference ecx(Register::ECX);
MemoryReference edx(Register::EDX);
MemoryReference ebx(Register::EBX);
MemoryReference esp(Register::ESP);
MemoryReference ebp(Register::EBP);
MemoryReference esi(Register::ESI);
MemoryReference edi(Register::EDI);
MemoryReference r8d(Register::R8D);
MemoryReference r9d(Register::R9D);
MemoryReference r10d(Register::R10D);
MemoryReference r11d(Register::R11D);
MemoryReference r12d(Register::R12D);
MemoryReference r13d(Register::R13D);
MemoryReference r14d(Register::R14D);
MemoryReference r15d(Register::R15D);
MemoryReference ax(Register::AX);
MemoryReference cx(Register::CX);
MemoryReference dx(Register::DX);
MemoryReference bx(Register::BX);
MemoryReference sp(Register::SP);
MemoryReference bp(Register::BP);
MemoryReference si(Register::SI);
MemoryReference di(Register::DI);
MemoryReference r8w(Register::R8W);
MemoryReference r9w(Register::R9W);
MemoryReference r10w(Register::R10W);
MemoryReference r11w(Register::R11W);
MemoryReference r12w(Register::R12W);
MemoryReference r13w(Register::R13W);
MemoryReference r14w(Register::R14W);
MemoryReference r15w(Register::R15W);
MemoryReference al(Register::AL);
MemoryReference cl(Register::CL);
MemoryReference dl(Register::DL);
MemoryReference bl(Register::BL);
MemoryReference ah(Register::AH);
MemoryReference ch(Register::CH);
MemoryReference dh(Register::DH);
MemoryReference bh(Register::BH);
MemoryReference r8b(Register::R8B);
MemoryReference r9b(Register::R9B);
MemoryReference r10b(Register::R10B);
MemoryReference r11b(Register::R11B);
MemoryReference r12b(Register::R12B);
MemoryReference r13b(Register::R13B);
MemoryReference r14b(Register::R14B);
MemoryReference r15b(Register::R15B);
MemoryReference spl(Register::SPL);
MemoryReference bpl(Register::BPL);
MemoryReference sil(Register::SIL);
MemoryReference dil(Register::DIL);
MemoryReference xmm0(Register::XMM0);
MemoryReference xmm1(Register::XMM1);
MemoryReference xmm2(Register::XMM2);
MemoryReference xmm3(Register::XMM3);
MemoryReference xmm4(Register::XMM4);
MemoryReference xmm5(Register::XMM5);
MemoryReference xmm6(Register::XMM6);
MemoryReference xmm7(Register::XMM7);
MemoryReference xmm8(Register::XMM8);
MemoryReference xmm9(Register::XMM9);
MemoryReference xmm10(Register::XMM10);
MemoryReference xmm11(Register::XMM11);
MemoryReference xmm12(Register::XMM12);
MemoryReference xmm13(Register::XMM13);
MemoryReference xmm14(Register::XMM14);
MemoryReference xmm15(Register::XMM15);


static inline bool is_extension_register(Register r) {
  return (static_cast<int8_t>(r) >= 8) && (static_cast<int8_t>(r) < 16);
}

static inline bool is_nonextension_byte_register(Register r) {
  return static_cast<int8_t>(r) >= 17;
}



void AMD64Assembler::reset() {
  this->name_to_label.clear();
  this->labels.clear();
  this->stream.clear();
}



void AMD64Assembler::write_raw(const string& data) {
  this->write(data);
}

void AMD64Assembler::write_raw(const void* data, size_t size) {
  this->write(string(reinterpret_cast<const char*>(data), size));
}



void AMD64Assembler::write_label(const string& name) {
  this->labels.emplace_back(name, this->stream.size());
  if (!this->name_to_label.emplace(name, &this->labels.back()).second) {
    throw invalid_argument("duplicate label name: " + name);
  }
}

void AMD64Assembler::write_label_address(const string& label_name) {
  string data("\0\0\0\0\0\0\0\0", 8);
  this->stream.emplace_back(data, label_name, 0, 8, true);
}



string AMD64Assembler::generate_rm(Operation op, const MemoryReference& mem,
    Register reg, OperandSize size, uint32_t extra_prefixes,
    bool skip_64bit_prefix) {
  uint32_t opcode = static_cast<uint32_t>(op);

  string ret;
  if (!mem.field_size) { // behavior = 3 (register reference)
    Register mem_base = mem.base_register;
    bool mem_ext = is_extension_register(mem_base);
    bool mem_nonext_byte = is_nonextension_byte_register(mem_base);
    bool reg_ext = is_extension_register(reg);
    bool reg_nonext_byte = is_nonextension_byte_register(reg);

    // using spl, bpl, sil, dil require a prefix of 0x40
    if (reg_nonext_byte) {
      reg = static_cast<Register>(reg - 13); // convert from enum value to register number
    }
    if (mem_nonext_byte) {
      mem_base = static_cast<Register>(mem_base - 13); // convert from enum value to register number
    }

    if (extra_prefixes) {
      ret += data_for_opcode(extra_prefixes);
    }

    uint8_t prefix_byte = 0x40 | (mem_ext ? 0x01 : 0) | (reg_ext ? 0x04 : 0);
    if (!skip_64bit_prefix &&
        ((size == OperandSize::QuadWord) || (size == OperandSize::QuadWordXMM))) {
      prefix_byte |= 0x08;
    }
    if ((size == OperandSize::Word) || (size == OperandSize::QuadWordXMM)) {
      ret += 0x66;
    }

    if (mem_nonext_byte || reg_nonext_byte || (prefix_byte != 0x40)) {
      ret += prefix_byte;
    }
    ret += data_for_opcode(opcode);
    ret += static_cast<char>(0xC0 | ((reg & 7) << 3) | (mem_base & 7));
    return ret;
  }

  // TODO: implement these cases
  if (mem.base_register == Register::None) {
    throw invalid_argument("memory references without base not supported");
  }

  bool reg_ext = is_extension_register(reg);
  bool mem_index_ext = is_extension_register(mem.index_register);
  bool mem_base_ext = is_extension_register(mem.base_register);

  uint8_t rm_byte = ((reg & 7) << 3);
  uint8_t sib_byte = 0x00;
  bool force_offset = false;
  if (mem.index_register != Register::None) {
    rm_byte |= 0x04;

    if (mem.field_size == 8) {
      sib_byte = 0xC0;
    } else if (mem.field_size == 4) {
      sib_byte = 0x80;
    } else if (mem.field_size == 2) {
      sib_byte = 0x40;
    } else if (mem.field_size != 1) {
      throw invalid_argument("field size must be 1, 2, 4, or 8");
    }

    if ((mem.base_register == Register::RBP) || (mem.base_register == Register::R13)) {
      force_offset = true;
    }
    if (mem.index_register == Register::RSP) {
      throw invalid_argument("RSP cannot be used as an index register in index addressing");
    }

    sib_byte |= mem.base_register & 7;
    if (mem.index_register == Register::None) {
      sib_byte |= ((Register::RSP & 7) << 3);
    } else {
      sib_byte |= ((mem.index_register & 7) << 3);
    }

    if (mem.base_register == Register::RIP) {
      throw invalid_argument("RIP cannot be used with scaled index addressing");
    }

  } else if ((mem.base_register == Register::RSP) ||
             (mem.base_register == Register::R12)) {
    rm_byte |= (mem.base_register & 7);
    sib_byte = 0x24;

  } else {
    if (mem.base_register == Register::RIP) {
      rm_byte |= 0x05;
    } else {
      rm_byte |= (mem.base_register & 7);
    }
  }

  // if no offset was given and the sib byte was not used and the base reg is
  // RBP or R13, then add a fake offset of 0 to the opcode (this is because this
  // case is shadowed by the RIP special case above)
  if ((mem.offset == 0) && ((rm_byte & 7) != 0x04) &&
      ((mem.base_register == Register::RSP) || (mem.base_register == Register::R13))) {
    force_offset = true;
  }

  // if an offset was given, update the behavior appropriately
  if ((mem.offset == 0) && !force_offset) {
    // behavior is 0; nothing to do
  } else if ((mem.offset <= 0x7F) && (mem.offset >= -0x80)) {
    rm_byte |= 0x40;
  } else if ((mem.offset <= 0x7FFFFFFFLL) && (mem.offset >= -0x80000000LL)) {
    rm_byte |= 0x80;
  } else {
    throw invalid_argument("offset must fit in 32 bits");
  }

  // fill in the ret string
  if (extra_prefixes) {
    ret += data_for_opcode(extra_prefixes);
  }

  uint8_t prefix_byte = 0x40 | (reg_ext ? 0x04 : 0) | (mem_index_ext ? 0x02 : 0) |
      (mem_base_ext ? 0x01 : 0);
  if ((size == OperandSize::QuadWord) || (size == OperandSize::QuadWordXMM)) {
    prefix_byte |= 0x08;
  }
  if ((size == OperandSize::Word) || (size == OperandSize::QuadWordXMM)) {
    ret += 0x66;
  }

  if (prefix_byte != 0x40) {
    ret += prefix_byte;
  }
  ret += data_for_opcode(opcode);
  ret += rm_byte;
  if ((rm_byte & 0x07) == 0x04) {
    ret += sib_byte;
  }
  if (rm_byte & 0x40) {
    ret.append(reinterpret_cast<const char*>(&mem.offset), 1);
  } else if ((rm_byte & 0x80) || (mem.base_register == Register::RIP)) {
    ret.append(reinterpret_cast<const char*>(&mem.offset), 4);
  }
  return ret;
}

string AMD64Assembler::generate_rm(Operation op, const MemoryReference& mem,
    uint8_t z, OperandSize size, uint32_t extra_prefixes,
    bool skip_64bit_prefix) {
  return AMD64Assembler::generate_rm(op, mem, static_cast<Register>(z), size,
      extra_prefixes, skip_64bit_prefix);
}

void AMD64Assembler::write_rm(Operation op, const MemoryReference& mem,
    Register reg, OperandSize size, uint32_t extra_prefixes,
    bool skip_64bit_prefix) {
  this->write(this->generate_rm(op, mem, reg, size, extra_prefixes,
      skip_64bit_prefix));
}

void AMD64Assembler::write_rm(Operation op, const MemoryReference& mem,
    uint8_t z, OperandSize size, uint32_t extra_prefixes,
    bool skip_64bit_prefix) {
  this->write(this->generate_rm(op, mem, z, size, extra_prefixes,
      skip_64bit_prefix));
}




Operation AMD64Assembler::load_store_oper_for_args(Operation op,
    const MemoryReference& to, const MemoryReference& from, OperandSize size) {
  return static_cast<Operation>(op | ((size != OperandSize::Byte) ? 1 : 0) |
      (from.field_size ? 2 : 0));
}

void AMD64Assembler::write_load_store(Operation base_op, const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  if (to.field_size && from.field_size) {
    throw invalid_argument("load/store opcodes can have at most one memory reference");
  }

  Operation op = load_store_oper_for_args(base_op, to, from, size);
  if (!from.field_size) {
    this->write_rm(op, to, from.base_register, size);
  } else {
    this->write_rm(op, from, to.base_register, size);
  }
}



void AMD64Assembler::write_int(uint8_t num) {
  if (num == 3) {
    this->write("\xCC");
  } else {
    string data("\xCD", 1);
    data += num;
    this->write(data);
  }
}

void AMD64Assembler::write_syscall() {
  this->write("\x0F\x05");
}



void AMD64Assembler::write_push(int64_t value) {
  string data;
  if ((value >= -0x80) && (value <= 0x7F)) {
    data += Operation::PUSH8;
    data += static_cast<int8_t>(value);
  } else if ((value >= -0x80000000LL) && (value <= 0x7FFFFFFFLL)) {
    data += Operation::PUSH32;
    data.append(reinterpret_cast<const char*>(&value), 4);
  } else {
    // emulate it with a push32 + mov32
    data += Operation::PUSH32;
    data.append(reinterpret_cast<const char*>(&value), 4);
    data += 0xC7;
    data += 0x44;
    data += 0x24;
    data += 0x04;
    data.append(reinterpret_cast<const char*>(&value) + 4, 4);
  }
  this->write(data);
}

void AMD64Assembler::write_push(Register r) {
  string data;
  if (is_extension_register(r)) {
    data += 0x41;
    data += (static_cast<uint8_t>(r) - 8) | 0x50;
  } else {
    data += r | 0x50;
  }
  this->write(data);
}

void AMD64Assembler::write_push(const MemoryReference& mem) {
  if (mem.field_size == 0) {
    string data;
    if (is_extension_register(mem.base_register)) {
      data += 0x41;
      data += (static_cast<uint8_t>(mem.base_register) - 8) | 0x50;
    } else {
      data += mem.base_register | 0x50;
    }
    this->write(data);

  } else {
    this->write_rm(Operation::PUSH_RM, mem, 6, OperandSize::DoubleWord);
  }
}

void AMD64Assembler::write_pop(Register r) {
  string data;
  if (is_extension_register(r)) {
    data += 0x41;
    data += (static_cast<uint8_t>(r) - 8) | 0x58;
  } else {
    data += r | 0x58;
  }
  this->write(data);
}

void AMD64Assembler::write_pop(const MemoryReference& mem) {
  if (!mem.field_size) {
    this->write_pop(mem.base_register);
  } else {
    this->write_rm(Operation::POP_RM, mem, 0, OperandSize::QuadWord);
  }
}



void AMD64Assembler::write_lea(Register r, const MemoryReference& mem) {
  this->write_rm(Operation::LEA, mem, r, OperandSize::QuadWord);
}

void AMD64Assembler::write_mov(const MemoryReference& to, const MemoryReference& from,
    OperandSize size) {
  this->write_load_store(Operation::MOV_STORE8, to, from, size);
}

void AMD64Assembler::write_mov(Register reg, const string& label_name) {
  string data;
  data += 0x48 | (is_extension_register(reg) ? 0x01 : 0);
  data += 0xB8 | (reg & 7);
  data.append("\0\0\0\0\0\0\0\0", 8);

  this->stream.emplace_back(data, label_name, data.size() - 8, 8, true);
}

void AMD64Assembler::write_mov(Register r, int64_t value, OperandSize size) {
  string data;
  if (size == OperandSize::QuadWord) {
    // if the value can fit in a r/m mov, use that instead
    if (((value & 0xFFFFFFFF80000000) == 0) || ((value & 0xFFFFFFFF80000000) == 0xFFFFFFFF80000000)) {
      data += 0x48 | (is_extension_register(r) ? 0x01 : 0);
      data += 0xC7;
      data += 0xC0 | (r & 7);
      data.append(reinterpret_cast<const char*>(&value), 4);
    } else {
      data += 0x48 | (is_extension_register(r) ? 0x01 : 0);
      data += 0xB8 | (r & 7);
      data.append(reinterpret_cast<const char*>(&value), 8);
    }

  } else if (size == OperandSize::DoubleWord) {
    string data;
    if (is_extension_register(r)) {
      data += 0x41;
    }
    data += 0xB8 | (r & 7);
    data.append(reinterpret_cast<const char*>(&value), 4);

  } else if (size == OperandSize::Word) {
    string data;
    data += 0x66;
    if (is_extension_register(r)) {
      data += 0x41;
    }
    data += 0xB8 | (r & 7);
    data.append(reinterpret_cast<const char*>(&value), 2);

  } else if (size == OperandSize::Byte) {
    string data;
    if (is_extension_register(r)) {
      data += 0x41;
    }
    data += 0xB0 | (r & 7);
    data += static_cast<int8_t>(value);

  } else {
    throw invalid_argument("unknown operand size");
  }
  this->write(data);
}

void AMD64Assembler::write_mov(const MemoryReference& mem, int64_t value,
    OperandSize size) {
  if (mem.field_size == 0) {
    this->write_mov(mem.base_register, value, size);

  } else {
    Operation op = (size == OperandSize::Byte) ? Operation::MOV_MEM8_IMM : Operation::MOV_MEM_IMM;
    string data = this->generate_rm(op, mem, 0, size);

    if (((value & 0xFFFFFFFF80000000) != 0) && ((value & 0xFFFFFFFF80000000) != 0xFFFFFFFF80000000)) {
      throw invalid_argument("value out of range for r/m mov");
    }

    // this opcode has a 32-bit imm for both 32-bit and 64-bit operand sizes
    if ((size == OperandSize::QuadWord) || (size == OperandSize::DoubleWord)) {
      data.append(reinterpret_cast<const char*>(&value), 4);
    } else if (size == OperandSize::Word) {
      data.append(reinterpret_cast<const char*>(&value), 2);
    } else if (size == OperandSize::Byte) {
      data += static_cast<int8_t>(value);
    } else {
      throw invalid_argument("unknown operand size");
    }
    this->write(data);
  }
}

void AMD64Assembler::write_xchg(Register reg, const MemoryReference& mem,
    OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::XCHG8 : Operation::XCHG;
  this->write_rm(op, mem, reg, size);
}

void AMD64Assembler::write_movzx8(Register reg, const MemoryReference& mem,
    OperandSize size) {
  this->write_rm(Operation::MOVZX8, mem, reg, size);
}

void AMD64Assembler::write_movzx16(Register reg, const MemoryReference& mem,
    OperandSize size) {
  this->write_rm(Operation::MOVZX16, mem, reg, size);
}

void AMD64Assembler::write_cmovo(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVO, from, to, size);
}

void AMD64Assembler::write_cmovno(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNO, from, to, size);
}

void AMD64Assembler::write_cmovb(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVB, from, to, size);
}

void AMD64Assembler::write_cmovnae(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNAE, from, to, size);
}

void AMD64Assembler::write_cmovc(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVC, from, to, size);
}

void AMD64Assembler::write_cmovnb(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNB, from, to, size);
}

void AMD64Assembler::write_cmovae(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVAE, from, to, size);
}

void AMD64Assembler::write_cmovnc(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNC, from, to, size);
}

void AMD64Assembler::write_cmovz(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVZ, from, to, size);
}

void AMD64Assembler::write_cmove(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVE, from, to, size);
}

void AMD64Assembler::write_cmovnz(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNZ, from, to, size);
}

void AMD64Assembler::write_cmovne(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNE, from, to, size);
}

void AMD64Assembler::write_cmovbe(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVBE, from, to, size);
}

void AMD64Assembler::write_cmovna(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNA, from, to, size);
}

void AMD64Assembler::write_cmovnbe(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNBE, from, to, size);
}

void AMD64Assembler::write_cmova(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVA, from, to, size);
}

void AMD64Assembler::write_cmovs(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVS, from, to, size);
}

void AMD64Assembler::write_cmovns(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNS, from, to, size);
}

void AMD64Assembler::write_cmovp(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVP, from, to, size);
}

void AMD64Assembler::write_cmovpe(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVPE, from, to, size);
}

void AMD64Assembler::write_cmovnp(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNP, from, to, size);
}

void AMD64Assembler::write_cmovpo(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVPO, from, to, size);
}

void AMD64Assembler::write_cmovl(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVL, from, to, size);
}

void AMD64Assembler::write_cmovnge(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNGE, from, to, size);
}

void AMD64Assembler::write_cmovnl(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNL, from, to, size);
}

void AMD64Assembler::write_cmovge(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVGE, from, to, size);
}

void AMD64Assembler::write_cmovle(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVLE, from, to, size);
}

void AMD64Assembler::write_cmovng(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNG, from, to, size);
}

void AMD64Assembler::write_cmovnle(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVNLE, from, to, size);
}

void AMD64Assembler::write_cmovg(Register to, const MemoryReference& from,
    OperandSize size) {
  this->write_rm(Operation::CMOVG, from, to, size);
}

void AMD64Assembler::write_movq_to_xmm(Register reg,
    const MemoryReference& from) {
  this->write_rm(Operation::MOVQ_TO_XMM, from, reg, OperandSize::QuadWordXMM);
}

void AMD64Assembler::write_movq_from_xmm(const MemoryReference& from,
    Register reg) {
  this->write_rm(Operation::MOVQ_FROM_XMM, from, reg, OperandSize::QuadWordXMM);
}

void AMD64Assembler::write_movsd(const MemoryReference& to,
    const MemoryReference& from) {
  if (to.field_size && from.field_size) {
    throw invalid_argument("load/store opcodes can have at most one memory reference");
  }

  if (!from.field_size) {
    this->write_rm(Operation::MOVSD_STORE, to, from.base_register,
        OperandSize::DoublePrecision, 0xF2);
  } else {
    this->write_rm(Operation::MOVSD_LOAD, from, to.base_register,
        OperandSize::DoublePrecision, 0xF2);
  }
}

void AMD64Assembler::write_addsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::ADDSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_subsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::SUBSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_mulsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::MULSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_divsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::DIVSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_minsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::MINSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_maxsd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::MAXSD, from, to, OperandSize::DoublePrecision, 0xF2);
}

void AMD64Assembler::write_cmpsd(Register to, const MemoryReference& from,
    uint8_t which) {
  string data = this->generate_rm(Operation::CMPSD, from, to,
      OperandSize::DoublePrecision, 0xF2);
  data += which;
  this->write(data);
}

void AMD64Assembler::write_cmpeqsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 0);
}

void AMD64Assembler::write_cmpltsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 1);
}

void AMD64Assembler::write_cmplesd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 2);
}

void AMD64Assembler::write_cmpunordsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 3);
}

void AMD64Assembler::write_cmpneqsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 4);
}

void AMD64Assembler::write_cmpnltsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 5);
}

void AMD64Assembler::write_cmpnlesd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 6);
}

void AMD64Assembler::write_cmpordsd(Register to, const MemoryReference& from) {
  this->write_cmpsd(to, from, 7);
}

void AMD64Assembler::write_roundsd(Register to, const MemoryReference& from,
    uint8_t mode) {
  string data = this->generate_rm(Operation::ROUNDSD, from, to,
      OperandSize::DoublePrecision, 0x66);
  data += mode;
  this->write(data);
}

void AMD64Assembler::write_cvtsi2sd(Register to, const MemoryReference& from) {
  this->write_rm(Operation::CVTSI2SD, from, to, OperandSize::QuadWord, 0xF2);
}

void AMD64Assembler::write_cvtsd2si(Register to, Register from) {
  this->write_rm(Operation::CVTSD2SI, MemoryReference(from), to,
      OperandSize::QuadWord, 0xF2);
}



void AMD64Assembler::write_nop() {
  static string nop("\x90", 1);
  this->write(nop);
}

void AMD64Assembler::write_jmp(const string& label_name) {
  this->stream.emplace_back(label_name, Operation::JMP8, Operation::JMP32);
}

void AMD64Assembler::write_jmp(const MemoryReference& mem) {
  this->write_rm(Operation::CALL_JMP_ABS, mem, 4, OperandSize::DoubleWord);
}

void AMD64Assembler::write_jmp_abs(const void* addr) {
  this->stream.emplace_back("", Operation::JMP8, Operation::JMP32,
      reinterpret_cast<int64_t>(addr));
}

string AMD64Assembler::generate_jmp(Operation op8, Operation op32,
    int64_t opcode_address, int64_t target_address, OperandSize* offset_size) {
  int64_t offset = target_address - opcode_address;

  if (op8 != Operation::NOP) { // may be omitted for call opcodes
    int64_t offset8 = offset - 2 - (static_cast<int64_t>(op8) > 0xFF);
    if ((offset8 >= -0x80) && (offset8 <= 0x7F)) {
      string data;
      if (op8 > 0xFF) {
        data += (op8 >> 8) & 0xFF;
      }
      data += op8 & 0xFF;
      data.append(reinterpret_cast<const char*>(&offset8), 1);
      if (offset_size) {
        *offset_size = OperandSize::Byte;
      }
      return data;
    }
  }

  int64_t offset32 = offset - 5 - (static_cast<int64_t>(op32) > 0xFF);
  if ((offset32 >= -0x80000000LL) && (offset32 <= 0x7FFFFFFFLL)) {
    string data;
    if (op32 > 0xFF) {
      data += (op32 >> 8) & 0xFF;
    }
    data += op32 & 0xFF;
    data.append(reinterpret_cast<const char*>(&offset32), 4);
    if (offset_size) {
      *offset_size = OperandSize::DoubleWord;
    }
    return data;
  }

  // the nasty case: we have to use a 64-bit offset. there doesn't seem to be a
  // good way to do this in amd64 assembly, so we'll emulate it with an absolute
  // call instead.
  if (offset_size) {
    *offset_size = OperandSize::QuadWord;
  }
  return AMD64Assembler::generate_absolute_jmp_position_independent(op8, op32,
      target_address);
}

string AMD64Assembler::generate_absolute_jmp_position_independent(Operation op8,
    Operation op32, int64_t target_address) {
  // only two cases are supported: absolute calls and absolute jumps
  // (conditional opcodes are not supported)

  string data;
  if ((op8 == Operation::JMP8) && (op32 == Operation::JMP32)) {
    // we emulate this by pushing the absolute address on the stack and
    // "returning" to it

    // push <low 4 bytes of address>
    data += 0x68;
    data.append(reinterpret_cast<const char*>(&target_address), 4);
    // mov [rsp + 4], <high 4 bytes of address>
    data += 0xC7;
    data += 0x44;
    data += 0x24;
    data += 0x04;
    data.append(reinterpret_cast<const char*>(&target_address) + 4, 4);
    // ret
    data += 0xC3;

  } else if ((op8 == Operation::NOP) && (op32 == Operation::CALL32)) {
    // TODO: implement this for realz. something like this might work:
    // call lolz
    // lolz:
    // add [rsp], <number of bytes between lolz and end of ret opcode>
    // push <low 4 bytes of address>
    // mov [rsp + 4], <high 4 bytes of address>
    // ret
    throw invalid_argument("position-independent absolute calls not yet implemented");

  } else {
    throw invalid_argument("position-independent absolute jumps may only use unconditional opcodes");
  }

  return data;
}

void AMD64Assembler::write_call(const string& label_name) {
  this->stream.emplace_back(label_name, Operation::NOP, Operation::CALL32);
}

void AMD64Assembler::write_call(const MemoryReference& mem) {
  this->write_rm(Operation::CALL_JMP_ABS, mem, 2, OperandSize::DoubleWord);
}

void AMD64Assembler::write_call_abs(const void* addr) {
  this->stream.emplace_back("", Operation::NOP, Operation::CALL32,
      reinterpret_cast<int64_t>(addr));
}

void AMD64Assembler::write_ret(uint16_t stack_bytes) {
  if (stack_bytes) {
    string data("\xC2", 1);
    data.append(reinterpret_cast<const char*>(&stack_bytes), 2);
    this->write(data);
  } else {
    this->write("\xC3");
  }
}



void AMD64Assembler::write_jcc(Operation op8, Operation op,
    const string& label_name) {
  this->stream.emplace_back(label_name, op8, op);
}

void AMD64Assembler::write_jo(const string& label_name) {
  this->write_jcc(Operation::JO8, Operation::JO, label_name);
}

void AMD64Assembler::write_jno(const string& label_name) {
  this->write_jcc(Operation::JNO8, Operation::JNO, label_name);
}

void AMD64Assembler::write_jb(const string& label_name) {
  this->write_jcc(Operation::JB8, Operation::JB, label_name);
}

void AMD64Assembler::write_jnae(const string& label_name) {
  this->write_jcc(Operation::JNAE8, Operation::JNAE, label_name);
}

void AMD64Assembler::write_jc(const string& label_name) {
  this->write_jcc(Operation::JC8, Operation::JC, label_name);
}

void AMD64Assembler::write_jnb(const string& label_name) {
  this->write_jcc(Operation::JNB8, Operation::JNB, label_name);
}

void AMD64Assembler::write_jae(const string& label_name) {
  this->write_jcc(Operation::JAE8, Operation::JAE, label_name);
}

void AMD64Assembler::write_jnc(const string& label_name) {
  this->write_jcc(Operation::JNC8, Operation::JNC, label_name);
}

void AMD64Assembler::write_jz(const string& label_name) {
  this->write_jcc(Operation::JZ8, Operation::JZ, label_name);
}

void AMD64Assembler::write_je(const string& label_name) {
  this->write_jcc(Operation::JE8, Operation::JE, label_name);
}

void AMD64Assembler::write_jnz(const string& label_name) {
  this->write_jcc(Operation::JNZ8, Operation::JNZ, label_name);
}

void AMD64Assembler::write_jne(const string& label_name) {
  this->write_jcc(Operation::JNE8, Operation::JNE, label_name);
}

void AMD64Assembler::write_jbe(const string& label_name) {
  this->write_jcc(Operation::JBE8, Operation::JBE, label_name);
}

void AMD64Assembler::write_jna(const string& label_name) {
  this->write_jcc(Operation::JNA8, Operation::JNA, label_name);
}

void AMD64Assembler::write_jnbe(const string& label_name) {
  this->write_jcc(Operation::JNBE8, Operation::JNBE, label_name);
}

void AMD64Assembler::write_ja(const string& label_name) {
  this->write_jcc(Operation::JA8, Operation::JA, label_name);
}

void AMD64Assembler::write_js(const string& label_name) {
  this->write_jcc(Operation::JS8, Operation::JS, label_name);
}

void AMD64Assembler::write_jns(const string& label_name) {
  this->write_jcc(Operation::JNS8, Operation::JNS, label_name);
}

void AMD64Assembler::write_jp(const string& label_name) {
  this->write_jcc(Operation::JP8, Operation::JP, label_name);
}

void AMD64Assembler::write_jpe(const string& label_name) {
  this->write_jcc(Operation::JPE8, Operation::JPE, label_name);
}

void AMD64Assembler::write_jnp(const string& label_name) {
  this->write_jcc(Operation::JNP8, Operation::JNP, label_name);
}

void AMD64Assembler::write_jpo(const string& label_name) {
  this->write_jcc(Operation::JPO8, Operation::JPO, label_name);
}

void AMD64Assembler::write_jl(const string& label_name) {
  this->write_jcc(Operation::JL8, Operation::JL, label_name);
}

void AMD64Assembler::write_jnge(const string& label_name) {
  this->write_jcc(Operation::JNGE8, Operation::JNGE, label_name);
}

void AMD64Assembler::write_jnl(const string& label_name) {
  this->write_jcc(Operation::JNL8, Operation::JNL, label_name);
}

void AMD64Assembler::write_jge(const string& label_name) {
  this->write_jcc(Operation::JGE8, Operation::JGE, label_name);
}

void AMD64Assembler::write_jle(const string& label_name) {
  this->write_jcc(Operation::JLE8, Operation::JLE, label_name);
}

void AMD64Assembler::write_jng(const string& label_name) {
  this->write_jcc(Operation::JNG8, Operation::JNG, label_name);
}

void AMD64Assembler::write_jnle(const string& label_name) {
  this->write_jcc(Operation::JNLE8, Operation::JNLE, label_name);
}

void AMD64Assembler::write_jg(const string& label_name) {
  this->write_jcc(Operation::JG8, Operation::JG, label_name);
}



void AMD64Assembler::write_imm_math(Operation math_op,
    const MemoryReference& to, int64_t value, OperandSize size) {
  if (math_op & 0xC7) {
    throw invalid_argument("immediate math opcodes must use basic Operation types");
  }

  Operation op;
  if (size == OperandSize::Byte) {
    op = Operation::MATH8_IMM8;
  } else if ((value < -0x80000000LL) || (value > 0x7FFFFFFFLL)) {
    throw invalid_argument("immediate value out of range");
  } else if ((value > 0x7F) || (value < -0x80)) {
    op = Operation::MATH_IMM32;
  } else {
    op = Operation::MATH_IMM8;
  }

  uint8_t z = (math_op >> 3) & 7;
  string data = this->generate_rm(op, to, z, size);
  if ((size == OperandSize::Byte) || (op == Operation::MATH_IMM8)) {
    data += static_cast<uint8_t>(value);
  } else if (size == OperandSize::Word) {
    data.append(reinterpret_cast<const char*>(&value), 2);
  } else {
    data.append(reinterpret_cast<const char*>(&value), 4);
  }
  this->write(data);
}



void AMD64Assembler::write_add(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::ADD_STORE8, to, from, size);
}

void AMD64Assembler::write_add(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::ADD_STORE8, to, value, size);
}

void AMD64Assembler::write_or(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::OR_STORE8, to, from, size);
}

void AMD64Assembler::write_or(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::OR_STORE8, to, value, size);
}

void AMD64Assembler::write_adc(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::ADC_STORE8, to, from, size);
}

void AMD64Assembler::write_adc(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::ADC_STORE8, to, value, size);
}

void AMD64Assembler::write_sbb(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::SBB_STORE8, to, from, size);
}

void AMD64Assembler::write_sbb(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::SBB_STORE8, to, value, size);
}

void AMD64Assembler::write_and(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::AND_STORE8, to, from, size);
}

void AMD64Assembler::write_and(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::AND_STORE8, to, value, size);
}

void AMD64Assembler::write_sub(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::SUB_STORE8, to, from, size);
}

void AMD64Assembler::write_sub(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::SUB_STORE8, to, value, size);
}

void AMD64Assembler::write_xor(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::XOR_STORE8, to, from, size);
}

void AMD64Assembler::write_xor(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::XOR_STORE8, to, value, size);
}

void AMD64Assembler::write_cmp(const MemoryReference& to,
    const MemoryReference& from, OperandSize size) {
  this->write_load_store(Operation::CMP_STORE8, to, from, size);
}

void AMD64Assembler::write_cmp(const MemoryReference& to, int64_t value,
    OperandSize size) {
  this->write_imm_math(Operation::CMP_STORE8, to, value, size);
}

void AMD64Assembler::write_shift(uint8_t which, const MemoryReference& mem,
    uint8_t bits, OperandSize size) {
  if (bits == 1) {
    Operation op = (size == OperandSize::Byte) ? Operation::SHIFT8_1 : Operation::SHIFT_1;
    this->write_rm(op, mem, which, size);
  } else if (bits != 0xFF) {
    Operation op = (size == OperandSize::Byte) ? Operation::SHIFT8_IMM : Operation::SHIFT_IMM;
    string data = this->generate_rm(op, mem, which, size);
    data += bits;
    this->write(data);
  } else {
    Operation op = (size == OperandSize::Byte) ? Operation::SHIFT8_CL : Operation::SHIFT_CL;
    this->write_rm(op, mem, which, size);
  }
}

void AMD64Assembler::write_rol(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(0, mem, bits, size);
}

void AMD64Assembler::write_ror(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(1, mem, bits, size);
}

void AMD64Assembler::write_rcl(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(2, mem, bits, size);
}

void AMD64Assembler::write_rcr(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(3, mem, bits, size);
}

void AMD64Assembler::write_shl(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(4, mem, bits, size);
}

void AMD64Assembler::write_shr(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(5, mem, bits, size);
}

void AMD64Assembler::write_sar(const MemoryReference& mem, uint8_t bits, OperandSize size) {
  this->write_shift(7, mem, bits, size);
}

void AMD64Assembler::write_rol_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(0, mem, 0xFF, size);
}

void AMD64Assembler::write_ror_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(1, mem, 0xFF, size);
}

void AMD64Assembler::write_rcl_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(2, mem, 0xFF, size);
}

void AMD64Assembler::write_rcr_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(3, mem, 0xFF, size);
}

void AMD64Assembler::write_shl_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(4, mem, 0xFF, size);
}

void AMD64Assembler::write_shr_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(5, mem, 0xFF, size);
}

void AMD64Assembler::write_sar_cl(const MemoryReference& mem, OperandSize size) {
  this->write_shift(7, mem, 0xFF, size);
}



void AMD64Assembler::write_not(const MemoryReference& target,
    OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, target, 2, size);
}

void AMD64Assembler::write_neg(const MemoryReference& target,
    OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, target, 3, size);
}

void AMD64Assembler::write_inc(const MemoryReference& target,
    OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::INC_DEC8 : Operation::INC_DEC;
  this->write_rm(op, target, 0, size);
}

void AMD64Assembler::write_dec(const MemoryReference& target,
    OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::INC_DEC8 : Operation::INC_DEC;
  this->write_rm(op, target, 1, size);
}

void AMD64Assembler::write_imul(Register target, const MemoryReference& mem,
    OperandSize size) {
  if (size == OperandSize::Byte) {
    throw invalid_argument("imul requires at least halfword operand size");
  }
  MemoryReference target_mem(target);
  this->write_load_store(Operation::IMUL, mem, target_mem, size);
}

void AMD64Assembler::write_mul(const MemoryReference& mem, OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, mem, 4, size);
}

void AMD64Assembler::write_imul(const MemoryReference& mem, OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, mem, 5, size);
}

void AMD64Assembler::write_div(const MemoryReference& mem, OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, mem, 6, size);
}

void AMD64Assembler::write_idiv(const MemoryReference& mem, OperandSize size) {
  Operation op = (size == OperandSize::Byte) ? Operation::NOT_NEG8 : Operation::NOT_NEG32;
  this->write_rm(op, mem, 7, size);
}



void AMD64Assembler::write_test(const MemoryReference& a,
    const MemoryReference& b, OperandSize size) {
  if (a.field_size && b.field_size) {
    throw invalid_argument("test opcode can have at most one memory reference");
  }

  Operation op = (size == OperandSize::Byte) ? Operation::TEST8 : Operation::TEST;
  if (a.field_size) {
    this->write_rm(op, a, b.base_register, size);
  } else {
    this->write_rm(op, b, a.base_register, size);
  }
}

void AMD64Assembler::write_test(const MemoryReference& a, int64_t value,
    OperandSize size) {
  if ((value < -0x80000000LL) || (value > 0x7FFFFFFFLL)) {
    throw invalid_argument("immediate value out of range");
  }

  Operation op = (size == OperandSize::Byte) ? Operation::TEST_IMM8 : Operation::TEST_IMM32;
  string data = this->generate_rm(op, a, 0, size);

  if (size == OperandSize::Byte) {
    data += static_cast<uint8_t>(value);
  } else if (size == OperandSize::Word) {
    data.append(reinterpret_cast<const char*>(&value), 2);
  } else {
    data.append(reinterpret_cast<const char*>(&value), 4);
  }
  this->write(data);
}

void AMD64Assembler::write_seto(const MemoryReference& target) {
  this->write_rm(Operation::SETO, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setno(const MemoryReference& target) {
  this->write_rm(Operation::SETNO, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setb(const MemoryReference& target) {
  this->write_rm(Operation::SETB, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnae(const MemoryReference& target) {
  this->write_rm(Operation::SETNAE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setc(const MemoryReference& target) {
  this->write_rm(Operation::SETC, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnb(const MemoryReference& target) {
  this->write_rm(Operation::SETNB, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setae(const MemoryReference& target) {
  this->write_rm(Operation::SETAE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnc(const MemoryReference& target) {
  this->write_rm(Operation::SETNC, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setz(const MemoryReference& target) {
  this->write_rm(Operation::SETZ, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_sete(const MemoryReference& target) {
  this->write_rm(Operation::SETE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnz(const MemoryReference& target) {
  this->write_rm(Operation::SETNZ, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setne(const MemoryReference& target) {
  this->write_rm(Operation::SETNE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setbe(const MemoryReference& target) {
  this->write_rm(Operation::SETBE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setna(const MemoryReference& target) {
  this->write_rm(Operation::SETNA, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnbe(const MemoryReference& target) {
  this->write_rm(Operation::SETNBE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_seta(const MemoryReference& target) {
  this->write_rm(Operation::SETA, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_sets(const MemoryReference& target) {
  this->write_rm(Operation::SETS, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setns(const MemoryReference& target) {
  this->write_rm(Operation::SETNS, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setp(const MemoryReference& target) {
  this->write_rm(Operation::SETP, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setpe(const MemoryReference& target) {
  this->write_rm(Operation::SETPE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnp(const MemoryReference& target) {
  this->write_rm(Operation::SETNP, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setpo(const MemoryReference& target) {
  this->write_rm(Operation::SETPO, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setl(const MemoryReference& target) {
  this->write_rm(Operation::SETL, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnge(const MemoryReference& target) {
  this->write_rm(Operation::SETNGE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnl(const MemoryReference& target) {
  this->write_rm(Operation::SETNL, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setge(const MemoryReference& target) {
  this->write_rm(Operation::SETGE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setle(const MemoryReference& target) {
  this->write_rm(Operation::SETLE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setng(const MemoryReference& target) {
  this->write_rm(Operation::SETNG, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setnle(const MemoryReference& target) {
  this->write_rm(Operation::SETNLE, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_setg(const MemoryReference& target) {
  this->write_rm(Operation::SETG, target, 0, OperandSize::Byte);
}

void AMD64Assembler::write_lock() {
  this->write("\xF0");
}



void AMD64Assembler::write(const string& data) {
  this->stream.emplace_back(data);
}

string AMD64Assembler::assemble(unordered_set<size_t>& patch_offsets,
    multimap<size_t, string>* label_offsets, int64_t base_address,
    bool autodefine_labels) {
  string code;

  // general strategy: assemble everything in order. for backward jumps, we know
  // exactly what the offset will be, so just use the right opcode. for forward
  // jumps, compute the offset as if all intervening jumps are 32-bit jumps, and
  // then backpatch the offset appropriately. this will waste space in some edge
  // cases but I'm lazy

  auto apply_patch = [&code, &patch_offsets](const Label& l, const Patch& p) {
    int64_t value = static_cast<int64_t>(l.byte_location);
    if (!p.absolute) {
      value -= (static_cast<int64_t>(p.where) + p.size);
    }

    // 8-bit patch
    if (p.size == 1) {
      if (p.absolute) {
        throw runtime_error("8-bit patches must be relative");
      }
      if ((value < -0x80) || (value > 0x7F)) {
        throw runtime_error("8-bit patch value out of range");
      }
      *reinterpret_cast<int8_t*>(&code[p.where]) = static_cast<int8_t>(value);

    // 32-bit patch
    } else if (p.size == 4) {
      if (p.absolute) {
        throw runtime_error("32-bit patches must be relative");
      }
      if ((value < -0x80000000LL) || (value > 0x7FFFFFFFLL)) {
        throw runtime_error("32-bit patch value out of range");
      }
      *reinterpret_cast<int32_t*>(&code[p.where]) = static_cast<int32_t>(value);

    // 64-bit patch
    } else if (p.size == 8) {
      if (p.absolute) {
        patch_offsets.emplace(p.where);
      }
      *reinterpret_cast<int64_t*>(&code[p.where]) = value;

    } else {
      throw invalid_argument("patch size is not 1, 4, or 8 bytes");
    }
  };

  size_t stream_location = 0;
  auto label_it = this->labels.begin();
  for (auto stream_it = this->stream.begin(); stream_it != this->stream.end(); stream_it++) {
    const auto& item = *stream_it;

    // if there's a label at this location, set its memory location
    while ((label_it != this->labels.end()) &&
        (label_it->stream_location == stream_location)) {
      label_it->byte_location = code.size();
      if (label_offsets) {
        label_offsets->emplace(label_it->byte_location, label_it->name);
      }

      // if there are patches waiting, perform them now that the label's
      // location is determined
      for (const auto& patch : label_it->patches) {
        apply_patch(*label_it, patch);
      }
      label_it->patches.clear();
      label_it++;
    }

    // if this stream item is a jump opcode, find the relevant label
    if (item.is_jump_call()) {
      if (item.absolute_target) {
        if (base_address) {
          code += this->generate_jmp(item.op8, item.op32,
              base_address + code.size(), item.absolute_target);
        } else {
          code += this->generate_absolute_jmp_position_independent(item.op8,
              item.op32, item.absolute_target);
        }

      } else {
        Label* label = NULL;
        try {
          label = this->name_to_label.at(item.data);
        } catch (const out_of_range& e) {
          if (autodefine_labels) {
            this->labels.emplace_back(item.data, 0);
            label = &this->labels.back();
            this->name_to_label.emplace(item.data, label);
          } else {
            throw runtime_error("nonexistent label: " + item.data);
          }
        }

        // if the label's address is known, we can easily write a jump opcode
        if (label->byte_location <= code.size()) {
          code += this->generate_jmp(item.op8, item.op32, code.size(),
              label->byte_location);

        // else, we have to estimate how far away the label will be
        } else {

          // find the target label
          size_t target_stream_location = label->stream_location;

          // find the maximum number of bytes between here and there, up to
          // approximately a byte boundary. there are only two kinds of
          // immediate jumps (8-bit and 32-bit), so if we can't use the 8-bit
          // one, we don't actually need to know the full displacement value.
          // this helps when assembling long streams with really long jumps.
          int64_t max_displacement = 0;
          size_t where_stream_location = stream_location;
          for (auto where_it = stream_it;
               (where_it != this->stream.end()) &&
                 (where_stream_location < target_stream_location) &&
                 (max_displacement < 0x0108);
               where_it++, where_stream_location++) {
            if (where_it->is_jump_call()) {
              // assume it's a 32-bit jump
              max_displacement += 5 + (where_it->op32 > 0xFF);
            } else {
              max_displacement += where_it->data.size();
            }
            string s = where_it->str();
          }

          // generate a bogus forward jmp opcode, and the appropriate patches
          OperandSize offset_size;
          code += this->generate_jmp(item.op8, item.op32, code.size(),
              code.size() + max_displacement, &offset_size);
          if (offset_size == OperandSize::Byte) {
            label->patches.emplace_back(code.size() - 1, 1, false);
          } else if (offset_size == OperandSize::DoubleWord) {
            label->patches.emplace_back(code.size() - 4, 4, false);
          } else {
            throw runtime_error("64-bit jump cannot be backpatched");
          }
        }
      }

    // this item is not a jump opcode; stick it in the buffer
    } else {
      code += item.data;
    }

    // if this stream item has a patch, apply it from the appropriate label
    if (item.patch.size) {
      // get the label that the patch refers to
      Label* label = NULL;
      try {
        label = this->name_to_label.at(item.patch_label_name);
      } catch (const out_of_range& e) {
        if (autodefine_labels) {
          this->labels.emplace_back(item.patch_label_name, 0);
          label = &this->labels.back();
          this->name_to_label.emplace(item.patch_label_name, label);
        } else {
          throw runtime_error("nonexistent label: " + item.patch_label_name);
        }
      }

      if (label) {
        // change the patch location so it's relative to the start of the code
        Patch p = item.patch;
        p.where += (code.size() - item.data.size());

        // if we know the label's location already, apply the patch now. if we
        // don't know the label's location, make the patch pending
        if (label->byte_location <= code.size()) {
          apply_patch(*label, p);
        } else {
          label->patches.emplace_back(p);
        }
      }
    }

    stream_location++;
  }

  // bugcheck: make sure there are no patches waiting. but if some labels were
  // autocreated, then allow incomplete patches
  if (!autodefine_labels) {
    for (const auto& label : this->labels) {
      if (!label.patches.empty()) {
        throw logic_error("some patches were not applied");
      }
    }
  }

  this->name_to_label.clear();
  this->labels.clear();
  this->stream.clear();

  return code;
}

AMD64Assembler::StreamItem::StreamItem(const string& data) : data(data),
    op8(Operation::NOP), op32(Operation::NOP), absolute_target(0),
    patch(0, 0, false) { }

AMD64Assembler::StreamItem::StreamItem(const string& data,
    const string& patch_label_name, size_t where, uint8_t size, bool absolute) :
    data(data), op8(Operation::NOP), op32(Operation::NOP), absolute_target(0),
    patch_label_name(patch_label_name), patch(where, size, absolute) { }

AMD64Assembler::StreamItem::StreamItem(const string& data, Operation op8,
    Operation op32, int64_t absolute_target) : data(data), op8(op8),
    op32(op32), absolute_target(absolute_target), patch(0, 0, false) { }

string AMD64Assembler::StreamItem::str() const {
  string data_str;
  if (this->is_jump_call()) {
    data_str = "\"" + this->data + "\"";
  } else {
    data_str = "[" + format_data_string(this->data) + "]";
  }

  string patch_str = this->patch.str();
  return string_printf("StreamItem(data=[%s], op8=%02X, op32=%02X, "
      "absolute_target=0x%" PRIX64 ", patch_label_name=%s, patch=%s)",
      data_str.c_str(), this->op8, this->op32, this->absolute_target,
      this->patch_label_name.c_str(), patch_str.c_str());
}

bool AMD64Assembler::StreamItem::is_jump_call() const {
  return (this->op8 != Operation::NOP) || (this->op32 != Operation::NOP);
}

AMD64Assembler::Patch::Patch(size_t where, uint8_t size, bool absolute) :
    where(where), size(size), absolute(absolute) { }

string AMD64Assembler::Patch::str() const {
  return string_printf("Patch(where=%zu, size=%hhu, absolute=%s)", this->where,
      this->size, this->absolute ? "true" : "false");
}

AMD64Assembler::Label::Label(const string& name, size_t stream_location) :
    name(name), stream_location(stream_location),
    byte_location(0xFFFFFFFFFFFFFFFF) { }

string AMD64Assembler::Label::str() const {
  return string_printf("Label(name=%s, stream_location=%zu, byte_location=%zu)",
      this->name.c_str(), this->stream_location, this->byte_location);
}



static inline Register make_reg(bool is_ext, uint8_t reg) {
  return static_cast<Register>((is_ext << 3) | reg);
}

static const char* math_op_names[] = {
    "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};

static const char* jmp_names[] = {
    "jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja",
    "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"};

string AMD64Assembler::disassemble(const string& data, size_t addr,
    const multimap<size_t, string>* label_offsets) {
  return AMD64Assembler::disassemble(data.data(), data.size(), addr,
      label_offsets);
}

string AMD64Assembler::disassemble(const void* vdata, size_t size,
    size_t addr, const multimap<size_t, string>* label_offsets) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(vdata);
  map<size_t, string> addr_to_text;
  multimap<size_t, string> addr_to_label;

  if (label_offsets) {
    for (const auto& it : *label_offsets) {
      addr_to_label.emplace(addr + it.first, it.second);
    }
  }

  uint64_t next_label = 0;

  // prefix flags
  bool ext = false;
  bool base_ext = false;
  bool index_ext = false;
  bool reg_ext = false;
  bool xmm_prefix = false;
  OperandSize operand_size = OperandSize::DoubleWord;

  size_t offset = 0;
  size_t opcode_start_offset = offset;
  for (size_t offset = 0; offset < size;) {
    uint8_t opcode = data[offset];
    offset++;

    // check for prefix bytes
    if ((opcode & 0xF0) == 0x40) {
      ext = true;
      base_ext = opcode & 1;
      index_ext = opcode & 2;
      reg_ext = opcode & 4;
      if (opcode & 8) {
        operand_size = OperandSize::QuadWord;
      }
      continue;
    }
    if (opcode == Operation::OPERAND16) {
      operand_size = OperandSize::Word;
      continue;
    }
    if (opcode == Operation::XMM_PREFIX) {
      xmm_prefix = true;
      continue;
    }

    string opcode_text;

    // check for extended opcodes
    if (opcode == 0x0F) {
      if (offset >= size) {
        opcode_text = "<<incomplete>>";
      } else {
        opcode = data[offset];
        offset++;

        if (opcode == 0x05) {
          opcode_text = "syscall";

        } else if ((opcode & 0xFE) == 0x10) {
          if (!xmm_prefix) {
            opcode_text = "<<unknown-0F-non-xmm>>";
          } else {
            opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                "movsd", (opcode == 0x10), NULL, ext, reg_ext, base_ext,
                index_ext, OperandSize::DoublePrecision);
          }

        } else if (opcode == 0x2A) {
          if (!xmm_prefix) {
            opcode_text = "<<unknown-0F-2A-non-xmm>>";
          } else {
            opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                "cvtsi2sd", true, NULL, ext, reg_ext, base_ext, index_ext,
                OperandSize::DoublePrecision);
          }

        } else if (opcode == 0x2C) {
          if (!xmm_prefix) {
            opcode_text = "<<unknown-0F-2C-non-xmm>>";
          } else {
            opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                "cvtsd2si", true, NULL, ext, reg_ext, base_ext, index_ext,
                OperandSize::DoublePrecision);
          }

        } else if (opcode == 0x3A) {
          if (xmm_prefix) {
            opcode_text = "<<unknown-0F-3A-xmm>>";
          } else if (offset >= size) {
            opcode_text = "<<incomplete-0F-3A-non-xmm>>";
          } else if (data[offset] != 0x0B) {
            opcode_text = string_printf("<<unknown-0F-3A-%02hhX-non-xmm>>",
                data[offset]);
            offset++;
          } else {
            offset++;

            opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                "roundsd", true, NULL, ext, reg_ext, base_ext, index_ext,
                OperandSize::DoublePrecision);
            if (offset >= size) {
              opcode_text += ", <<incomplete>>";
            } else {
              opcode_text += string_printf(", 0x%02hhX", data[offset]);
              offset++;
            }
          }

        } else if ((opcode & 0xF8) == 0x58) {
          if (!xmm_prefix) {
            opcode_text = "<<unknown-0F-58-non-xmm>>";
          } else {
            static const char* names[8] = {
                "addsd", "mulsd", NULL, NULL, "subsd", "minsd", "divsd", "maxsd"};
            const char* name = names[opcode & 7];
            if (!name) {
              opcode_text = "<<unknown-0F-58-xmm>>";
            } else {
              opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                  name, true, NULL, ext, reg_ext, base_ext, index_ext,
                  OperandSize::DoublePrecision);
            }
          }

        } else if ((opcode & 0xF0) == 0x40) {
          static const char* names[0x10] = {
              "cmovo", "cmovno", "cmovb", "cmovae",
              "cmove", "cmovne", "cmovbe", "cmova",
              "cmovs", "cmovns", "cmovp", "cmovnp",
              "cmovl", "cmovge", "cmovle", "cmovg",
          };
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              names[opcode & 0x0F], true, NULL, ext, reg_ext, base_ext,
              index_ext, operand_size);

        } else if (opcode == 0x6E) {
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              "movq", true, NULL, ext, reg_ext, base_ext,
              index_ext, OperandSize::QuadWordXMM, OperandSize::QuadWord);

        } else if (opcode == 0x7E) {
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              "movq", false, NULL, ext, reg_ext, base_ext,
              index_ext, OperandSize::QuadWordXMM, OperandSize::QuadWord);

        } else if ((opcode & 0xF0) == 0x80) {
          opcode_text = AMD64Assembler::disassemble_jmp(data, size, offset,
              addr, jmp_names[opcode & 0x0F], true, addr_to_label, next_label);

        } else if ((opcode & 0xF0) == 0x90) {
          static const char* names[] = {
              "seto", "setno", "setb", "setae", "sete", "setne", "setbe",
              "seta", "sets", "setns", "setp", "setnp", "setl", "setge",
              "setle", "setg"};
          static const char* fake_names[] = {
              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              names[opcode & 0x0F], false, fake_names, ext, reg_ext, base_ext,
              index_ext, OperandSize::Byte);

        } else if (opcode == 0xAF) {
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              "imul", true, NULL, ext, reg_ext, base_ext, index_ext,
              operand_size);

        } else if ((opcode & 0xFE) == 0xB6) {
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
              "movzx", true, NULL, ext, reg_ext, base_ext, index_ext,
              operand_size, (opcode & 1) ? OperandSize::Word : OperandSize::Byte);

        } else if (opcode == 0xC2) {
          if (!xmm_prefix) {
            opcode_text = "<<unknown-0F-C2-non-xmm>>";
          } else {
            opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
                "cmpsd", true, NULL, ext, reg_ext, base_ext, index_ext,
                OperandSize::DoublePrecision);
            if (offset >= size) {
              opcode_text += ", <<incomplete>>";
            } else {
              const char* operation_suffixes[8] = {
                  "eq", "lt", "le", "unord", "ne", "ge", "gt", "ord"};
              if (data[offset] < 8) {
                opcode_text += string_printf(", %s", operation_suffixes[data[offset]]);
              } else {
                opcode_text += string_printf(", 0x%hhX", data[offset]);
              }
              offset++;
            }
          }

        } else {
          opcode_text = "<<unknown-0F>>";
        }
      }

    } else if ((opcode & 0xC0) == 0x00) {
      if (opcode & 0x04) {
        // this is one of the al/rax/imm instructions
        opcode_text = "<<a-reg math>>";
      } else {
        uint8_t which = (opcode >> 3) & 7;
        if (!(opcode & 1)) {
          operand_size = OperandSize::Byte;
        }
        opcode_text = AMD64Assembler::disassemble_rm(data, size, offset,
            math_op_names[which], opcode & 2, NULL, ext, reg_ext, base_ext,
            index_ext, operand_size);
      }

    } else if ((opcode & 0xF0) == 0x50) {
      Register reg = make_reg(base_ext, opcode & 7);
      if (opcode & 0x08) {
        opcode_text = string_printf("pop      %s", name_for_register(reg));
      } else {
        opcode_text = string_printf("push     %s", name_for_register(reg));
      }

    } else if (opcode == 0x68) {
      if (offset >= size - 3) {
        opcode_text += ", <<incomplete>>";
      } else {
        int64_t value = *reinterpret_cast<const int32_t*>(&data[offset]);
        opcode_text = string_printf("push     %s0x%02X",
            (value < 0) ? "-" : "", (value < 0) ? -value : value);
        offset += 4;
      }

    } else if (opcode == 0x6A) {
      if (offset >= size) {
        opcode_text += ", <<incomplete>>";
      } else {
        int8_t signed_data = static_cast<int8_t>(data[offset]);
        bool negative = signed_data < 0;
        opcode_text = string_printf("push     %s0x%02X",
            negative ? "-" : "", negative ? -signed_data : signed_data);
        offset += 1;
      }

    } else if ((opcode & 0xF0) == 0x70) {
      if (offset >= size) {
        opcode_text = "<<incomplete>>";
      } else {
        opcode_text = AMD64Assembler::disassemble_jmp(data, size, offset, addr,
            jmp_names[opcode & 0x0F], false, addr_to_label, next_label);
      }

    } else if ((opcode & 0xFC) == 0x80) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
          true, math_op_names, ext, reg_ext, base_ext, index_ext, operand_size);

      uint32_t displacement;
      if ((opcode & 3) == 1) {
        displacement = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += 4;
      } else {
        displacement = static_cast<uint8_t>(data[offset]);
        offset++;
      }
      opcode_text += string_printf(", %" PRIu32, displacement);

    } else if ((opcode & 0xFE) == 0x84) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, "test",
          true, NULL, ext, reg_ext, base_ext, index_ext, operand_size);

    } else if ((opcode & 0xFE) == 0x86) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, "xchg",
          true, NULL, ext, reg_ext, base_ext, index_ext, operand_size);

    } else if ((opcode & 0xFC) == 0x88) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, "mov",
          opcode & 2, NULL, ext, reg_ext, base_ext, index_ext, operand_size);

    } else if (opcode == 0x8D) {
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, "lea",
          true, NULL, ext, reg_ext, base_ext, index_ext, operand_size);

    } else if (opcode == 0x8F) {
      static const char* names[] = {
          "pop", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
          false, names, ext, reg_ext, base_ext, index_ext, OperandSize::QuadWord);

    } else if (opcode == 0x90) {
      opcode_text = "nop";

    } else if ((opcode & 0xF8) == 0xB8) {
      Register reg = make_reg(reg_ext, opcode & 7);
      string reg_name = name_for_register(reg, operand_size);
      opcode_text = string_printf("movabs   %s, ", reg_name.c_str());
      opcode_text += AMD64Assembler::disassemble_imm(data, size, offset, operand_size, true);

    } else if (((opcode & 0xFC) == 0xD0) || ((opcode & 0xFE) == 0xC0)) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      static const char* names[] = {
          "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"};
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
          true, names, ext, reg_ext, base_ext, index_ext, operand_size);

      if ((opcode & 0xFE) == 0xC0) {
        if (offset >= size) {
          opcode_text += ", <<incomplete>>";
        } else {
          opcode_text += string_printf(", %" PRIu8, data[offset]);
          offset++;
        }
      } else if (opcode & 2) {
        opcode_text += ", cl";
      } else {
        opcode_text += ", 1";
      }

    } else if (opcode == 0xC3) {
      opcode_text = "ret";

    } else if ((opcode & 0xFE) == 0xC6) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      static const char* names[] = {
          "mov", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
      opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
          false, names, ext, reg_ext, base_ext, index_ext, operand_size);
      opcode_text += ", " + AMD64Assembler::disassemble_imm(data, size, offset,
          operand_size);

    } else if ((opcode & 0xFE) == 0xCC) {
      if (opcode & 1) {
        if (offset >= size) {
          opcode_text = "int      <<incomplete>>";
        } else {
          opcode_text = string_printf("int      %" PRIu8, data[offset]);
          offset++;
        }
      } else {
        opcode_text = "int      0x03";
      }

    } else if ((opcode & 0xFC) == 0xE8) {
      opcode_text = AMD64Assembler::disassemble_jmp(data, size, offset, addr,
          (opcode & 1) ? "jmp" : "call", !(opcode & 2), addr_to_label,
          next_label);

    } else if (opcode == 0xF0) {
      opcode_text = "lock";

    } else if ((opcode & 0xFE) == 0xF6) {
      if (!(opcode & 1)) {
        operand_size = OperandSize::Byte;
      }
      if (offset >= size) {
        opcode_text = "<<incomplete test/not/neg>>";
      } else {
        uint8_t subcode = (data[offset] & 0x38) >> 3;

        static const char* names[] = {
            "test", "test", "not", "neg", "mul", "imul", "div", "idiv"};
        opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
            true, names, ext, reg_ext, base_ext, index_ext, operand_size);

        // if it's a test (0 or 1) then an immediate value follows
        if ((subcode & 1) == subcode) {
          opcode_text += ", ";
          opcode_text += AMD64Assembler::disassemble_imm(data, size, offset, operand_size);
        }
      }

    } else if ((opcode & 0xFE) == 0xFE) {
      if (offset >= size) {
        opcode_text = "<<incomplete inc/dec/call/jmp/push>>";
      } else {
        // FE only valid for inc/dec (not call/jmp/push)
        bool is_inc_dec = (((data[offset] >> 3) & 7) < 2);
        if (!is_inc_dec && (opcode == 0xFE)) {
          opcode_text = "<<invalid FE>>";

        } else {
          static const char* names[] = {
            "inc", "dec", "call", NULL, "jmp", NULL, "push", NULL};
          // this opcode has an implied 64-bit operand size except for inc/dec
          if (opcode == 0xFE) {
            operand_size = OperandSize::Byte;
          } else if (!is_inc_dec) {
            operand_size = OperandSize::QuadWord;
          }
          opcode_text = AMD64Assembler::disassemble_rm(data, size, offset, NULL,
              false, names, ext, reg_ext, base_ext, index_ext, operand_size);
        }
      }

    } else {
      opcode_text = "<<unknown-value>>";
    }

    string line = string_printf("%016" PRIX64 "  ", addr + opcode_start_offset);
    {
      size_t x;
      for (x = opcode_start_offset; x < offset; x++) {
        line += string_printf(" %02" PRIX8, data[x]);
      }
      for (; x < opcode_start_offset + 10; x++) {
        line += "   ";
      }
    }
    line += "   ";
    line += opcode_text;

    addr_to_text.emplace(addr + opcode_start_offset, line);

    ext = false;
    base_ext = false;
    index_ext = false;
    reg_ext = false;
    xmm_prefix = false;
    operand_size = OperandSize::DoubleWord;
    opcode_start_offset = offset;
  }

  string result;
  auto label_it = addr_to_label.begin();
  for (const auto& it : addr_to_text) {
    while ((label_it != addr_to_label.end()) && (label_it->first <= it.first)) {
      if (label_it->first != it.first) {
        result += string_printf("%s: ; (misaligned at %016" PRIX64 ")\n",
            label_it->second.c_str(), label_it->first);
      } else {
        result += string_printf("%s:\n", label_it->second.c_str());
      }
      label_it = addr_to_label.erase(label_it);
    }
    result += it.second;
    result += '\n';
  }
  return result;
}

string AMD64Assembler::disassemble_rm(const uint8_t* data, size_t size,
    size_t& offset, const char* opcode_name, bool is_load,
    const char** op_name_table, bool ext, bool reg_ext, bool base_ext,
    bool index_ext, OperandSize reg_operand_size, OperandSize mem_operand_size) {
  if (offset >= size) {
    return "<<incomplete>>";
  }

  if (mem_operand_size == OperandSize::Automatic) {
    mem_operand_size = reg_operand_size;
  }

  uint8_t rm = data[offset];
  uint8_t behavior = (rm >> 6) & 3;
  uint8_t subtype = (rm >> 3) & 7; // only used for single-argument opcodes
  Register base_reg = make_reg(base_ext, rm & 7);
  Register reg = make_reg(reg_ext, subtype);
  offset++;

  string reg_str = name_for_register(reg, reg_operand_size);

  string mem_str;
  if (behavior == 3) {
    // convert *h regs to spl, etc. if the extension prefix is given
    if ((mem_operand_size == OperandSize::Byte) && ext) {
      if (base_reg >= 4 && base_reg <= 7) {
        base_reg = static_cast<Register>(base_reg + 13);
      }
    }
    mem_str = name_for_register(base_reg, mem_operand_size);

  } else {
    // this special case uses RIP instead of RBP
    if ((behavior == 0) && (base_reg == Register::RBP)) {
      behavior = 2; // also has a 32-bit offset
      mem_str = "[rip";
    } else if ((base_reg == Register::RSP) || (base_reg == Register::R12)) {
      if (offset >= size) {
        return "<<incomplete>>";
      }

      uint8_t sib = data[offset];
      offset++;
      uint8_t scale = 1 << ((sib >> 6) & 3);
      base_reg = make_reg(base_ext, sib & 7);
      Register index_reg = make_reg(index_ext, (sib >> 3) & 7);
      if ((behavior == 0) && ((base_reg == Register::RBP) || (base_reg == Register::R13))) {
        // for behavior=0, there's no base reg
        mem_str = "[";
      } else {
        mem_str = string_printf("[%s", name_for_register(base_reg, OperandSize::QuadWord));
      }

      if (index_reg != Register::RSP) {
        mem_str += string_printf(" + %s", name_for_register(index_reg, OperandSize::QuadWord));
        if (scale != 1) {
          mem_str += string_printf(" * %" PRIu8, scale);
        }
      }

    } else {
      mem_str = string_printf("[%s", name_for_register(base_reg, OperandSize::QuadWord));
    }

    // add the offset, if given
    if (behavior == 1) {
      if (offset >= size) {
        return "<<incomplete>>";
      }
      int8_t displacement = static_cast<int8_t>(data[offset]);
      offset++;

      if (displacement & 0x80) {
        mem_str += string_printf(" - 0x%" PRIX8, -displacement);
      } else if (displacement) {
        mem_str += string_printf(" + 0x%" PRIX8, displacement);
      }

    } else if (behavior == 2) {
      if (offset >= size - 3) {
        return "<<incomplete>>";
      }
      int32_t displacement = *reinterpret_cast<const int32_t*>(&data[offset]);
      offset += 4;

      if (displacement & 0x80000000) {
        mem_str += string_printf(" - 0x%" PRIX32, -displacement);
      } else if (displacement) {
        mem_str += string_printf(" + 0x%" PRIX32, displacement);
      }
    }

    mem_str += ']';
  }

  // if the operand size of the memory operand isn't obvious, prefix it
  if (mem_str[0] == '[' && ((mem_operand_size != reg_operand_size) || op_name_table)) {
    static const vector<const char*> size_names({
        "byte", "word", "dword", "qword", "float", "double"});
    mem_str = string(size_names.at(mem_operand_size)) + " ptr " + mem_str;
  }

  if (op_name_table) {
    const char* name = op_name_table[subtype] ? op_name_table[subtype] : opcode_name;
    if (!name) {
      return "<<unknown-name>>";
    }
    return string_printf("%-8s %s", name, mem_str.c_str());
  }

  if (is_load) {
    return string_printf("%-8s %s, %s", opcode_name, reg_str.c_str(), mem_str.c_str());
  } else {
    return string_printf("%-8s %s, %s", opcode_name, mem_str.c_str(), reg_str.c_str());
  }
}

string AMD64Assembler::disassemble_jmp(const uint8_t* data, size_t size,
    size_t& offset, uint64_t addr, const char* opcode_name, bool is_32bit,
    multimap<size_t, string>& addr_to_label, uint64_t& next_label) {

  int32_t displacement;
  if ((offset >= size) || (is_32bit && (offset >= size - 3))) {
    return "<<incomplete>>";
  }

  if (is_32bit) {
    displacement = *reinterpret_cast<const int32_t*>(&data[offset]);
    offset += 4;
  } else {
    displacement = static_cast<int8_t>(data[offset]);
    offset++;
  }

  // find the label name, creating it if it doesn't exist
  uint64_t label_addr = addr + offset + displacement;
  auto label_its = addr_to_label.equal_range(label_addr);
  if (label_its.first == label_its.second) {
    label_its.first = addr_to_label.emplace(label_addr,
        string_printf("label%" PRId64, next_label++));
    label_its.second = label_its.first;
    label_its.second++;
  }

  string label_names;
  for (; label_its.first != label_its.second; label_its.first++) {
    if (!label_names.empty()) {
      label_names += ", ";
    }
    label_names += label_its.first->second;
  }

  if (displacement < 0) {
    return string_printf("%-8s -0x%" PRIX32 " ; %s",
        opcode_name, -displacement, label_names.c_str());
  } else {
    return string_printf("%-8s +0x%" PRIX32 " ; %s",
        opcode_name, displacement, label_names.c_str());
  }
}

string AMD64Assembler::disassemble_imm(const uint8_t* data, size_t size,
    size_t& offset, OperandSize operand_size, bool allow_64bit) {
  if (!allow_64bit && (operand_size == OperandSize::QuadWord)) {
    operand_size = OperandSize::DoubleWord;
  }

  string opcode_text;
  if (operand_size == OperandSize::QuadWord) {
    if (offset >= size - 7) {
      opcode_text = "<<incomplete imm64>>";
    } else {
      opcode_text = string_printf("0x%016" PRIX64,
          *reinterpret_cast<const uint64_t*>(&data[offset]));
      offset += 8;
    }
  } else if (operand_size == OperandSize::DoubleWord) {
    if (offset >= size - 3) {
      opcode_text = "<<incomplete imm32>>";
    } else {
      opcode_text = string_printf("0x%08" PRIX32,
          *reinterpret_cast<const uint32_t*>(&data[offset]));
      offset += 4;
    }
  } else if (operand_size == OperandSize::Word) {
    if (offset >= size - 1) {
      opcode_text = "<<incomplete imm16>>";
    } else {
      opcode_text = string_printf("0x%04" PRIX16,
          *reinterpret_cast<const uint16_t*>(&data[offset]));
      offset += 2;
    }
  } else if (operand_size == OperandSize::Byte) {
    if (offset >= size) {
      opcode_text = "<<incomplete imm8>>";
    } else {
      opcode_text = string_printf("0x%02" PRIX8, data[offset]);
      offset += 1;
    }
  }
  return opcode_text;
}