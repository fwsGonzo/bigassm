#pragma once
#include "assembler.hpp"
#include "rv32i_instr.hpp"
#include <functional>
using Instruction = riscv::rv32i_instruction;

using InstructionList = std::vector<Instruction>;

struct Opcode
{
	std::function<InstructionList(Assembler&)> handler;
};

struct Opcodes
{
	static Token opcode(const std::string&);
};
