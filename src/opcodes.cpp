#include "opcodes.hpp"
#include "instruction_list.hpp"
#include <unordered_map>

static void build_uint32(
	InstructionList& res, int reg, int32_t value)
{
	Instruction i1(RV32I_OP_IMM);
	i1.Itype.rd = reg;
	i1.Itype.imm = value;
	/* If the constant is large, we can turn the
	   LI into ADDI combined with LUI. */
	Instruction i2(RV32I_LUI);
	i2.Utype.rd = reg;
	i2.Utype.imm = (value + i1.Itype.imm) >> 12;
	if (i2.Utype.imm) {
		i1.Itype.rs1 = reg; /* Turn into ADDI */
		res.push_back(i2);
		/* Slight optimization to avoid ADDI */
		if (value & 0xFFF)
			res.push_back(i1);
	} else {
		res.push_back(i1);
	}
}

static Opcode OP_LI {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg = a.next<TK_REGISTER> ();
		auto& imm = a.next<TK_CONSTANT> ();

		InstructionList res;
		build_uint32(res, reg.i64, imm.i64);
		return res;
	}
};
static Opcode OP_SET {
	.handler = [] (Assembler& a) -> InstructionList {
		InstructionList res;
		auto& dst = a.next<TK_REGISTER> ();
		auto& temp = a.next<TK_REGISTER> ();
		auto& imm = a.next<TK_CONSTANT> ();
		/* When the constant is 32-bits */
		if (imm.u128 < 0x100000000) {
			build_uint32(res, dst.i64, imm.i64);
			return res;
		}
		/* Large constants using intermediate register */
		union {
			__int128_t whole;
			int32_t    imm[4];
		} value;
		value.whole = imm.u128;
		build_uint32(res, dst.i64, value.imm[3]);
		__uint128_t value_so_far = value.imm[3];

		for (int i = 2; i >= 0; i--)
		{
			const auto imm = value.imm[i];
			build_uint32(res, temp.i64, imm);
			if (value_so_far != 0) {
				/* dst <<= 32 */
				Instruction i3(RV32I_OP_IMM);
				i3.Itype.rd  = dst.i64;
				i3.Itype.rs1 = dst.i64;
				i3.Itype.funct3 = 0x1;
				i3.Itype.imm = 32;
				res.push_back(i3);
			}
			value_so_far <<= 32;
			value_so_far |= imm;
			/* dst += temp */
			Instruction i4(RV32I_OP);
			i4.Rtype.rd  = dst.i64;
			i4.Rtype.rs1 = dst.i64;
			i4.Rtype.rs2 = temp.i64;
			res.push_back(i4);
		}
		return res;
	}
};

static Opcode OP_LA {
	.handler = [] (Assembler& a) -> InstructionList
	{
		auto& reg = a.next<TK_REGISTER> ();
		auto& lbl = a.next<TK_SYMBOL> ();
		Instruction i1(RV32I_LUI);
		i1.Itype.rd = reg.i64;
		Instruction i2(RV32I_OP_IMM);
		i2.Itype.rd = reg.i64;
		i2.Itype.rs1 = reg.i64;
		/* Potentially resolve later */
		a.schedule(lbl,
		[iaddr = a.current_address()] (Assembler& a, address_t addr) {
			auto& i1 = a.instruction_at(iaddr + 0);
			auto& i2 = a.instruction_at(iaddr + 4);
			i1.Utype.imm = addr >> 12;
			i2.Itype.imm = addr & 0xFFF;
		});
		return {i1, i2};
	}
};

static Opcode OP_LQ {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& src = a.next<TK_REGISTER> ();
		Instruction i1(RV32I_LOAD);
		i1.Itype.funct3 = 0x7;
		i1.Itype.rs1 = src.i64;
		if (a.next_is(TK_CONSTANT)) {
			auto& imm = a.next<TK_CONSTANT> ();
			i1.Itype.imm = imm.i64;
		}
		auto& dst = a.next<TK_REGISTER> ();
		i1.Itype.rd  = dst.i64;
		return {i1};
	}
};

static Opcode OP_SQ {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& src = a.next<TK_REGISTER> ();
		auto& dst = a.next<TK_REGISTER> ();
		Instruction i1(RV32I_STORE);
		i1.Stype.rs2 = src.i64;
		i1.Stype.funct3 = 0x4;
		i1.Stype.rs1 = dst.i64;
		if (a.next_is(TK_CONSTANT)) {
			auto& imm = a.next<TK_CONSTANT> ();
			i1.Stype.imm1 = imm.i64;
			i1.Stype.imm2 = imm.i64 >> 5;
		}
		return {i1};
	}
};

static InstructionList branch_helper(Assembler& a, uint32_t f3)
{
	auto& reg1 = a.next<TK_REGISTER> ();
	auto& reg2 = a.next<TK_REGISTER> ();
	auto& lbl = a.next<TK_SYMBOL> ();
	Instruction instr(RV32I_BRANCH);
	instr.Btype.rs1 = reg1.i64;
	instr.Btype.rs2 = reg2.i64;
	instr.Btype.funct3 = f3;
	a.schedule(lbl,
	[iaddr = a.current_address()] (Assembler& a, address_t addr) {
		auto& instr = a.instruction_at(iaddr);
		const int32_t diff = addr - iaddr;
		instr.Btype.imm2 = diff >> 1;
		instr.Btype.imm3 = diff >> 5;
		instr.Btype.imm1 = diff >> 11;
		instr.Btype.imm4 = diff >> 12;
	});
	return {instr};
}
static Opcode OP_BEQ {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x0)};
	}
};
static Opcode OP_BNE {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x1)};
	}
};
static Opcode OP_BLT {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x4)};
	}
};
static Opcode OP_BGE {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x5)};
	}
};
static Opcode OP_BLTU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x6)};
	}
};
static Opcode OP_BGEU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x7)};
	}
};

static Opcode OP_FARCALL {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg = a.next<TK_REGISTER> ();
		auto& lbl = a.next<TK_SYMBOL> ();

		Instruction i1(RV32I_LUI);
		i1.Utype.rd = reg.i64;
		Instruction i2(RV32I_JALR);
		i2.Itype.rs1 = reg.i64;
		i2.Itype.rd  = 1; /* Return address */

		a.schedule(lbl,
		[iaddr = a.current_address()] (Assembler& a, address_t addr) {
			auto& i1 = a.instruction_at(iaddr + 0);
			auto& i2 = a.instruction_at(iaddr + 4);
			i1.Utype.imm = addr >> 12;
			i2.Itype.imm = addr;
		});
		return {i1, i2};
	}
};
static Opcode OP_CALL {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& lbl = a.next<TK_SYMBOL> ();
		Instruction instr(RV32I_JAL);
		instr.Jtype.rd = 1; /* Return address */
		a.schedule(lbl,
		[iaddr = a.current_address()] (Assembler& a, address_t addr) {
			auto& instr = a.instruction_at(iaddr);
			const int32_t diff = addr - iaddr;
			instr.Jtype.imm3 = diff >> 1;
			instr.Jtype.imm2 = diff >> 11;
			instr.Jtype.imm1 = diff >> 12;
			instr.Jtype.imm4 = diff >> 19;
		});
		return {instr};
	}
};
static Opcode OP_JALR {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg1 = a.next<TK_REGISTER> ();
		Instruction instr(RV32I_JALR);
		instr.Itype.rs1 = reg1.i64;
		if (a.next_is(TK_REGISTER)) {
			auto& reg2 = a.next<TK_REGISTER> ();
			instr.Itype.rd = reg2.i64;
		}
		return {instr};
	}
};
static Opcode OP_RET {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_JALR);
		instr.Itype.rs1 = 1;
		return {instr};
	}
};
static Opcode OP_JMP {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& lbl = a.next<TK_SYMBOL> ();
		Instruction instr(RV32I_JAL);
		a.schedule(lbl,
		[iaddr = a.current_address()] (Assembler& a, address_t addr) {
			auto& instr = a.instruction_at(iaddr);
			const int32_t diff = addr - iaddr;
			instr.Jtype.imm3 = diff >> 1;
			instr.Jtype.imm2 = diff >> 11;
			instr.Jtype.imm1 = diff >> 12;
			instr.Jtype.imm4 = diff >> 19;
		});
		return {instr};
	}
};

static Instruction op_imm_helper(Assembler& a, uint32_t funct3)
{
	auto& reg = a.next<TK_REGISTER> ();
	Instruction instr(RV32I_OP_IMM);
	if (a.next_is(TK_CONSTANT)) {
		auto& imm = a.next<TK_CONSTANT> ();
		if (imm.i64 > 0x7FF || imm.i64 < -2048)
			a.token_exception(imm, "Out of bounds immediate value");

		instr.Itype.rd  = reg.i64;
		instr.Itype.funct3 = funct3;
		instr.Itype.rs1 = reg.i64;
		instr.Itype.imm = imm.i64;
	} else if (a.next_is(TK_REGISTER)) {
		auto& reg2 = a.next<TK_REGISTER> ();

		instr.Rtype.opcode = RV32I_OP;
		instr.Rtype.rd  = reg.i64;
		instr.Rtype.funct3 = funct3;
		instr.Rtype.rs1 = reg.i64;
		instr.Rtype.rs2 = reg2.i64;
	} else {
		a.token_exception(a.next(), "Unexpected token");
	}
	return instr;
}
static Instruction op_f7_helper(Assembler& a, uint32_t f3, uint32_t f7)
{
	auto& reg = a.next<TK_REGISTER> ();
	auto& reg2 = a.next<TK_REGISTER> ();
	Instruction instr(RV32I_OP);
	instr.Rtype.rd  = reg.i64;
	instr.Rtype.funct3 = f3;
	instr.Rtype.rs1 = reg.i64;
	instr.Rtype.rs2 = reg2.i64;
	instr.Rtype.funct7 = f7;
	return instr;
}

static Opcode OP_ADD {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x0)};
	}
};
static Opcode OP_SLL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x1)};
	}
};
static Opcode OP_SLT {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x2)};
	}
};
static Opcode OP_SLTU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x3)};
	}
};
static Opcode OP_SRL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x5)};
	}
};
static Opcode OP_AND {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x7)};
	}
};
static Opcode OP_OR {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x6)};
	}
};
static Opcode OP_XOR {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, 0x4)};
	}
};

static Opcode OP_SUB {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x0, 0b0100000)};
	}
};
static Opcode OP_MUL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x0, 0x1)};
	}
};
static Opcode OP_DIV {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x4, 0x1)};
	}
};
static Opcode OP_DIVU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x5, 0x1)};
	}
};
static Opcode OP_REM {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x6, 0x1)};
	}
};
static Opcode OP_REMU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, 0x7, 0x1)};
	}
};

static Opcode OP_ECALL {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		return {instr};
	}
};
static Opcode OP_EBREAK {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.imm = 0x1;
		return {instr};
	}
};
static Opcode OP_WFI {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.imm = 0x105;
		return {instr};
	}
};
static Opcode OP_SYSTEM {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& f3  = a.next<TK_CONSTANT> ();
		auto& imm = a.next<TK_CONSTANT> ();
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.funct3 = f3.i64;
		instr.Itype.imm = imm.i64;
		return {instr};
	}
};

static const std::unordered_map<std::string, Opcode> opcode_list =
{
	{"set", OP_SET},
	{"li", OP_LI},
	{"la", OP_LA},
	{"lq", OP_LQ},
	{"sq", OP_SQ},

	{"beq", OP_BEQ},
	{"bne", OP_BNE},
	{"blt", OP_BLT},
	{"bge", OP_BGE},
	{"bltu", OP_BLTU},
	{"bgeu", OP_BGEU},

	{"farcall", OP_FARCALL},
	{"call", OP_CALL},
	{"jal", OP_CALL},
	{"jalr", OP_JALR},
	{"ret", OP_RET},
	{"jmp", OP_JMP},

	{"add", OP_ADD},
	{"sub", OP_SUB},
	{"sll", OP_SLL},
	{"slt", OP_SLT},
	{"sltu", OP_SLTU},
	{"srl", OP_SRL},
	{"and", OP_AND},
	{"or",  OP_OR},
	{"xor", OP_XOR},

	{"mul",  OP_MUL},
	{"div",  OP_DIV},
	{"divu", OP_DIVU},
	{"rem",  OP_REM},
	{"remu", OP_REMU},

	{"syscall",OP_ECALL},
	{"ecall",  OP_ECALL},
	{"ebreak", OP_EBREAK},
	{"wfi",    OP_WFI},
	{"system", OP_SYSTEM},
};

Token Opcodes::opcode(const std::string& value)
{
	Token tk;
	tk.value = value;

	auto it = opcode_list.find(value);
	if (it != opcode_list.end())
	{
		tk.type = TK_OPCODE;
		tk.opcode = &it->second;
		return tk;
	}
	tk.type = TK_SYMBOL;
	return tk;
}
