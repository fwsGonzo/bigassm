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
		Instruction i1(RV32I_AUIPC);
		i1.Itype.rd = reg.i64;
		Instruction i2(RV32I_OP_IMM);
		i2.Itype.rd = reg.i64;
		i2.Itype.rs1 = reg.i64;
		/* Potentially resolve later */
		a.schedule(lbl,
		[iaddr = a.current_address()] (Assembler& a, address_t addr) {
			auto& i1 = a.instruction_at(iaddr + 0);
			auto& i2 = a.instruction_at(iaddr + 4);
			addr -= iaddr;
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
	{"jmp", OP_JMP},
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
