#include "opcodes.hpp"
#include "instruction_list.hpp"
#include <unordered_map>

static Opcode OP_LI {
	.handler = [] (Assembler& a) -> InstructionList {
		InstructionList res;
		auto& reg = a.next<TK_REGISTER> ();
		auto& off = a.next<TK_CONSTANT> ();

		Instruction i1(RV32I_OP_IMM);
		i1.Itype.rd = reg.i64;
		i1.Itype.imm = off.i64;
		/* If the constant is large, we can turn the
		   LI into ADDI combined with LUI. */
		if (off.i64 > 0x7FF || off.i64 < -2048)
		{
			i1.Itype.rs1 = reg.i64;
			Instruction i2(RV32I_LUI);
			i2.Utype.rd = reg.i64;
			i2.Utype.imm = off.i64 >> 12;
			res.push_back(i2);
		}
		if (off.i64 & 0x7FF)
			res.push_back(i1);
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

static Opcode OP_SCALL {
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

static const std::unordered_map<std::string, Opcode> opcode_list =
{
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

	{"scall",  OP_SCALL},
	{"ebreak", OP_EBREAK},
	{"wfi",    OP_WFI},
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
