#pragma once
#include <cstdint>
#include <string>
#include <vector>

using address_t = __uint128_t;
extern std::string to_hex_string(address_t);

struct RawToken {
	std::string name;
	uint32_t line;
};

enum TokenType {
	TK_DIRECTIVE,
	TK_LABEL,
	TK_STRING,
	TK_SYMBOL,
	TK_OPCODE,
	TK_PSEUDOOP,
	TK_REGISTER,
	TK_CONSTANT,
	TK_UNSPEC,
};

struct Register {
	uint8_t n;
};

struct Opcode;

struct PseudoOp;

struct Token {
	std::string value;
	enum TokenType type;
	uint32_t line = 0;
	union {
		address_t addr;
		int64_t   i64;
		uint64_t  u64;
		__uint128_t  u128;
		const Opcode* opcode;
		const PseudoOp* pseudoop;
		char      raw[16];
	};

	bool is_opcode() const noexcept { return type == TK_OPCODE; }
	bool is_register() const noexcept { return type == TK_REGISTER; }
	bool is_symbol() const noexcept { return type == TK_SYMBOL; }

	std::string to_string() const;
	static std::string to_string(TokenType);

	Token(TokenType tt = TK_UNSPEC) : type(tt), addr(0) {}
};

namespace riscv {
	union rv32i_instruction;
}
using Instruction = riscv::rv32i_instruction;
