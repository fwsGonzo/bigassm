#include "opcodes.hpp"
#include "section.hpp"
#include "instruction_list.hpp"
#include <unordered_map>

static bool is_relatively_close(Assembler&, int64_t diff)
{
	return (diff >= INT32_MIN && diff <= INT32_MAX);
}
static void bounds_check_jump(Assembler& a, int64_t diff)
{
	if (diff < -2048 || diff > 2047) {
		[[unlikely]];
		Token imm(TK_SYMBOL);
		imm.addr = diff;
		imm.value = std::to_string(diff);
		a.token_exception(imm, "Out of bounds address for jump");
	}
}

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

static struct Opcode OP_NOP {
	.handler = [] (Assembler&) -> InstructionList {
		return {Instruction(RV32I_OP_IMM)};
	}
};

static struct Opcode OP_LI {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg = a.next<TK_REGISTER> ();
		auto imm = a.resolve_constants();

		InstructionList res;
		build_uint32(res, reg.i64, imm.i64);
		return res;
	}
};
static struct Opcode OP_SET {
	.handler = [] (Assembler& a) -> InstructionList {
		InstructionList res;
		auto& dst = a.next<TK_REGISTER> ();
		auto& temp = a.next<TK_REGISTER> ();
		auto imm = a.resolve_constants();
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

static struct Opcode OP_LA {
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
		[loc = a.current_location()] (Assembler& a, auto&, auto& sym) {
			auto& i1 = a.instruction_at(loc, 0);
			auto& i2 = a.instruction_at(loc, 4);
			int64_t diff = sym.address() - loc.address();
			if (is_relatively_close(a, diff)) {
				i2.Itype.imm = diff;
				i1.Utype.opcode = RV32I_AUIPC;
				i1.Utype.imm = (diff + i2.Itype.imm) >> 12;
			} else {
				/* TODO: Bounds-check */
				i2.Itype.imm = sym.address();
				i1.Utype.imm = (sym.address() + i2.Itype.imm) >> 12;
			}
		});
		return {i1, i2};
	}
};

static InstructionList load_helper(Assembler& a, uint32_t f3)
{
	auto& src = a.next<TK_REGISTER> ();
	Instruction i1(RV32I_LOAD);
	i1.Itype.funct3 = f3;
	i1.Itype.rs1 = src.i64;
	if (a.next_is(TK_CONSTANT)) {
		auto imm = a.resolve_constants();
		i1.Itype.imm = imm.i64;
	}
	auto& dst = a.next<TK_REGISTER> ();
	i1.Itype.rd  = dst.i64;
	return {i1};
}
static struct Opcode OP_LB {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x0);
	}
};
static struct Opcode OP_LH {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x1);
	}
};
static struct Opcode OP_LW {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x2);
	}
};
static struct Opcode OP_LD {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x3);
	}
};
static struct Opcode OP_LBU {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x4);
	}
};
static struct Opcode OP_LHU {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x5);
	}
};
static struct Opcode OP_LWU {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x6);
	}
};
static struct Opcode OP_LDU {
	.handler = [] (Assembler&) -> InstructionList {
		throw std::runtime_error("Unimplemented");
	}
};
static struct Opcode OP_LQ {
	.handler = [] (Assembler& a) -> InstructionList {
		return load_helper(a, 0x7);
	}
};

static InstructionList store_helper(Assembler& a, uint32_t f3)
{
	auto& src = a.next<TK_REGISTER> ();
	auto& dst = a.next<TK_REGISTER> ();
	Instruction i1(RV32I_STORE);
	i1.Stype.rs1 = dst.i64;
	i1.Stype.funct3 = f3;
	i1.Stype.rs2 = src.i64;
	if (a.next_is(TK_CONSTANT)) {
		auto imm = a.resolve_constants();
		i1.Stype.imm1 = imm.i64;
		i1.Stype.imm2 = imm.i64 >> 5;
		}
	return {i1};
}
static struct Opcode OP_SB {
	.handler = [] (Assembler& a) -> InstructionList {
		return store_helper(a, 0x0);
	}
};
static struct Opcode OP_SH {
	.handler = [] (Assembler& a) -> InstructionList {
		return store_helper(a, 0x1);
	}
};
static struct Opcode OP_SW {
	.handler = [] (Assembler& a) -> InstructionList {
		return store_helper(a, 0x2);
	}
};
static struct Opcode OP_SD {
	.handler = [] (Assembler& a) -> InstructionList {
		return store_helper(a, 0x3);
	}
};
static struct Opcode OP_SQ {
	.handler = [] (Assembler& a) -> InstructionList {
		return store_helper(a, 0x4);
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
	[loc = a.current_location()] (Assembler& a, auto&, auto& sym) {
		auto& instr = a.instruction_at(loc);
		const int32_t diff = sym.address() - loc.address();
		instr.Btype.imm2 = diff >> 1;
		instr.Btype.imm3 = diff >> 5;
		instr.Btype.imm1 = diff >> 11;
		instr.Btype.imm4 = diff >> 12;
	});
	return {instr};
}
static struct Opcode OP_BEQ {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x0)};
	}
};
static struct Opcode OP_BNE {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x1)};
	}
};
static struct Opcode OP_BLT {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x4)};
	}
};
static struct Opcode OP_BGE {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x5)};
	}
};
static struct Opcode OP_BLTU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x6)};
	}
};
static struct Opcode OP_BGEU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {branch_helper(a, 0x7)};
	}
};

static struct Opcode OP_FARCALL {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg = a.next<TK_REGISTER> ();
		auto& lbl = a.next<TK_SYMBOL> ();

		Instruction i1(RV32I_LUI);
		i1.Utype.rd = reg.i64;
		Instruction i2(RV32I_JALR);
		i2.Itype.rs1 = reg.i64;
		i2.Itype.rd  = 1; /* Return address */

		a.schedule(lbl,
		[loc = a.current_location()] (Assembler& a, auto&, auto& sym) {
			auto& i1 = a.instruction_at(loc, 0);
			auto& i2 = a.instruction_at(loc, 4);
			i1.Utype.imm = sym.address() >> 12;
			i2.Itype.imm = sym.address();
		});
		return {i1, i2};
	}
};
static struct Opcode OP_CALL {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& lbl = a.next<TK_SYMBOL> ();
		Instruction instr(RV32I_JAL);
		instr.Jtype.rd = 1; /* Return address */
		a.schedule(lbl,
		[loc = a.current_location()] (Assembler& a, auto&, auto& sym) {
			auto& instr = a.instruction_at(loc);
			const int64_t diff = sym.address() - loc.address();
			bounds_check_jump(a, diff);
			instr.Jtype.imm3 = diff >> 1;
			instr.Jtype.imm2 = diff >> 11;
			instr.Jtype.imm1 = diff >> 12;
			instr.Jtype.imm4 = diff >> 19;
		});
		return {instr};
	}
};
static struct Opcode OP_JALR {
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
static struct Opcode OP_RET {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_JALR);
		instr.Itype.rs1 = 1;
		return {instr};
	}
};
static struct Opcode OP_JMP {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& lbl = a.next<TK_SYMBOL> ();
		Instruction instr(RV32I_JAL);
		a.schedule(lbl,
		[loc = a.current_location()] (Assembler& a, auto&, auto& sym) {
			auto& instr = a.instruction_at(loc);
			const int64_t diff = sym.address() - loc.address();
			bounds_check_jump(a, diff);
			instr.Jtype.imm3 = diff >> 1;
			instr.Jtype.imm2 = diff >> 11;
			instr.Jtype.imm1 = diff >> 12;
			instr.Jtype.imm4 = diff >> 19;
		});
		return {instr};
	}
};

static Instruction op_imm_helper(Assembler& a, uint32_t opcode, uint32_t funct3)
{
	auto& reg = a.next<TK_REGISTER> ();
	Instruction instr(opcode);
	if (a.next_is(TK_CONSTANT)) {
		auto imm = a.resolve_constants();
		if (imm.i64 > 0x7FF || imm.i64 < -2048)
			a.token_exception(imm, "Out of bounds immediate value");

		instr.Itype.rd  = reg.i64;
		instr.Itype.funct3 = funct3;
		instr.Itype.rs1 = reg.i64;
		instr.Itype.imm = imm.i64;
	} else if (a.next_is(TK_REGISTER)) {
		auto& reg2 = a.next<TK_REGISTER> ();

		instr.Rtype.opcode =
			(opcode == RV32I_OP_IMM) ? RV32I_OP :
			(opcode == RV64I_OP_IMM32) ? RV64I_OP32 : RV128I_OP_IMM64;
		instr.Rtype.rd  = reg.i64;
		instr.Rtype.funct3 = funct3;
		instr.Rtype.rs1 = reg.i64;
		instr.Rtype.rs2 = reg2.i64;
	} else {
		a.token_exception(a.next(), "Unexpected token");
	}
	return instr;
}
static Instruction op_f7_helper(Assembler& a, uint32_t opcode, uint32_t f3, uint32_t f7)
{
	auto& reg = a.next<TK_REGISTER> ();
	auto& reg2 = a.next<TK_REGISTER> ();
	Instruction instr(opcode);
	instr.Rtype.rd  = reg.i64;
	instr.Rtype.funct3 = f3;
	instr.Rtype.rs1 = reg.i64;
	instr.Rtype.rs2 = reg2.i64;
	instr.Rtype.funct7 = f7;
	return instr;
}

static struct Opcode OP_MOV {
	.handler = [] (Assembler& a) -> InstructionList {
		auto& reg = a.next<TK_REGISTER> ();
		Instruction instr(RV32I_OP);
		auto& reg2 = a.next<TK_REGISTER> ();
		instr.Rtype.rd  = reg.i64;
		instr.Rtype.funct3 = 0x0;
		instr.Rtype.rs1 = reg2.i64;
		instr.Rtype.rs2 = 0;
		return {instr};
	}
};

template <unsigned Opcode>
static struct Opcode OP_ADD {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x0)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_SLL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x1)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_SLT {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x2)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_SLTU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x3)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_SRL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x5)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_AND {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x7)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_OR {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x6)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_XOR {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_imm_helper(a, Opcode, 0x4)};
	}
};

template <unsigned Opcode>
static struct Opcode OP_SUB {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x0, 0b0100000)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_MUL {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x0, 0x1)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_DIV {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x4, 0x1)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_DIVU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x5, 0x1)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_REM {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x6, 0x1)};
	}
};
template <unsigned Opcode>
static struct Opcode OP_REMU {
	.handler = [] (Assembler& a) -> InstructionList {
		return {op_f7_helper(a, Opcode, 0x7, 0x1)};
	}
};

static struct Opcode OP_SYSCALL {
	.handler = [] (Assembler& a) -> InstructionList {
		auto imm = a.resolve_constants();
		Instruction i1(RV32I_OP_IMM);
		i1.Itype.rd = 17;
		i1.Itype.imm = imm.i64;
		Instruction i2(RV32I_SYSTEM);
		return {i1, i2};
	}
};
static struct Opcode OP_ECALL {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		return {instr};
	}
};
static struct Opcode OP_EBREAK {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.imm = 0x1;
		return {instr};
	}
};
static struct Opcode OP_WFI {
	.handler = [] (Assembler&) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.imm = 0x105;
		return {instr};
	}
};
static struct Opcode OP_SYSTEM {
	.handler = [] (Assembler& a) -> InstructionList {
		auto f3  = a.resolve_constants();
		auto imm = a.resolve_constants();
		Instruction instr(RV32I_SYSTEM);
		instr.Itype.funct3 = f3.i64;
		instr.Itype.imm = imm.i64;
		return {instr};
	}
};

static const std::unordered_map<std::string, Opcode> opcode_list =
{
	{"nop", OP_NOP},
	{"set", OP_SET},
	{"li", OP_LI},
	{"la", OP_LA},
	{"mv", OP_MOV},
	{"mov", OP_MOV},

	{"lb", OP_LB},
	{"lh", OP_LH},
	{"lw", OP_LW},
	{"ld", OP_LD},
	{"lq", OP_LQ},
	{"lbu", OP_LBU},
	{"lhu", OP_LHU},
	{"lwu", OP_LWU},
	{"ldu", OP_LDU},

	{"sb", OP_SB},
	{"sh", OP_SH},
	{"sw", OP_SW},
	{"sd", OP_SD},
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

	{"add", OP_ADD<RV32I_OP_IMM>},
	{"sub", OP_SUB<RV32I_OP>},
	{"sll", OP_SLL<RV32I_OP_IMM>},
	{"slt", OP_SLT<RV32I_OP_IMM>},
	{"sltu", OP_SLTU<RV32I_OP_IMM>},
	{"srl", OP_SRL<RV32I_OP_IMM>},
	{"and", OP_AND<RV32I_OP_IMM>},
	{"or",  OP_OR<RV32I_OP_IMM>},
	{"xor", OP_XOR<RV32I_OP_IMM>},

	{"addw", OP_ADD<RV64I_OP_IMM32>},
	{"subw", OP_SUB<RV64I_OP32>},
	{"sllw", OP_SLL<RV64I_OP_IMM32>},
	{"sltw", OP_SLT<RV64I_OP_IMM32>},
	{"sltuw", OP_SLTU<RV64I_OP_IMM32>},
	{"srlw", OP_SRL<RV64I_OP_IMM32>},
	{"andw", OP_AND<RV64I_OP_IMM32>},
	{"orw",  OP_OR<RV64I_OP_IMM32>},
	{"xorw", OP_XOR<RV64I_OP_IMM32>},

	{"addd", OP_ADD<RV128I_OP_IMM64>},
	{"subd", OP_SUB<RV128I_OP64>},
	{"slld", OP_SLL<RV128I_OP_IMM64>},
	{"sltd", OP_SLT<RV128I_OP_IMM64>},
	{"sltud", OP_SLTU<RV128I_OP_IMM64>},
	{"srld", OP_SRL<RV128I_OP_IMM64>},
	{"andd", OP_AND<RV128I_OP_IMM64>},
	{"ord",  OP_OR<RV128I_OP_IMM64>},
	{"xord", OP_XOR<RV128I_OP_IMM64>},

	{"mul",  OP_MUL<RV32I_OP>},
	{"div",  OP_DIV<RV32I_OP>},
	{"divu", OP_DIVU<RV32I_OP>},
	{"rem",  OP_REM<RV32I_OP>},
	{"remu", OP_REMU<RV32I_OP>},

	{"mulw",  OP_MUL<RV64I_OP32>},
	{"divw",  OP_DIV<RV64I_OP32>},
	{"divuw", OP_DIVU<RV64I_OP32>},
	{"remw",  OP_REM<RV64I_OP32>},
	{"remuw", OP_REMU<RV64I_OP32>},

	{"muld",  OP_MUL<RV128I_OP64>},
	{"divd",  OP_DIV<RV128I_OP64>},
	{"divud", OP_DIVU<RV128I_OP64>},
	{"remd",  OP_REM<RV128I_OP64>},
	{"remud", OP_REMU<RV128I_OP64>},

	{"syscall",OP_SYSCALL},
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
