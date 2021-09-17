#include <unordered_map>
#include "instruction_list.hpp"

static Opcode OP_LI {
	.handler = [] (Assembler& a) -> InstructionList {
		Instruction instr(RV32I_OP_IMM);
		auto& reg = a.next<TK_REGISTER> ();
		auto& off = a.next<TK_CONSTANT> ();
		instr.Itype.rd = reg.i64;
		instr.Itype.imm = off.i64;
		return {instr};
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
		/* Resolve later */
		if (!a.symbol_is_known(lbl)) {
			a.schedule(lbl,
			[iaddr = a.current_address()] (Assembler& a, address_t addr) {
				auto& i1 = a.instruction_at(iaddr + 0);
				auto& i2 = a.instruction_at(iaddr + 4);
				addr -= iaddr;
				i1.Utype.imm = addr >> 12;
				i2.Itype.imm = addr & 0xFFF;
			});
		}
		return {i1, i2};
	}
};

static Opcode OP_SCALL {
	.handler = [] (Assembler& a) -> InstructionList {
		Instruction instr(RV32I_SYSTEM);
		return {instr};
	}
};

static Opcode OP_LQ {
	.handler = [] (Assembler& a) -> InstructionList {
		Instruction i1(RV32I_LOAD);
		auto& src = a.next<TK_REGISTER> ();
		auto& dst = a.next<TK_REGISTER> ();
		auto& imm = a.next<TK_CONSTANT> ();
		i1.Stype.rs1 = dst.i64;
		return {i1};
	}
};

static Opcode OP_SQ {
	.handler = [] (Assembler& a) -> InstructionList {
		Instruction i1(RV32I_STORE);
		auto& src = a.next<TK_REGISTER> ();
		auto& dst = a.next<TK_REGISTER> ();
		auto& imm = a.next<TK_CONSTANT> ();
		i1.Stype.rs2 = src.i64;
		i1.Stype.funct3 = 0x4;
		i1.Stype.rs1 = dst.i64;
		union {
			struct {
				uint16_t imm1 : 5;
				uint16_t imm2 : 7 ;
			};
			uint16_t imm;
		} simm;
		simm.imm = imm.i64;
		i1.Stype.imm1 = simm.imm1;
		i1.Stype.imm2 = simm.imm2;
		return {i1};
	}
};

static const std::unordered_map<std::string, Opcode> opcode_list =
{
	{"li", OP_LI},
	{"la", OP_LA},
	{"lq", OP_LQ},
	{"sq", OP_SQ},
	{"scall", OP_SCALL},
};
