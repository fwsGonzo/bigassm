#include "assembler.hpp"
#include "opcodes.hpp"
#include <cassert>

static size_t to_hex(char* buffer, size_t len, __uint128_t value)
{
	if (len < 32) return 0;
	len = 8; /* At least print 8 hex digits */
	static constexpr char lut[] = "0123456789ABCDEF";
	for (unsigned i = 0; i < 16 - len / 2; i++) {
		if ((value >> ((15-i) * 8)) & 0xFF) {
			len = 32 - i * 2;
			break;
		}
	}
	const size_t max = len / 2;
	for (unsigned i = 0; i < max; i++) {
		buffer[i*2 + 0] = lut[(value >> ((max-1-i) * 8 + 4)) & 0xF];
		buffer[i*2 + 1] = lut[(value >> ((max-1-i) * 8 + 0)) & 0xF];
	}
	return len;
}

void Assembler::assemble()
{
	assert((base_address() & 0xFFF) == 0);

	while (!this->done())
	{
		auto& token = this->next();
		switch (token.type) {
		case TK_DIRECTIVE:
			this->directive(token);
			break;
		case TK_LABEL: {
			char buffer[32];
			int len = to_hex(buffer, sizeof(buffer), current_address());
			printf("Label %s at address 0x%.*s\n",
				token.value.c_str(), len, buffer);
			/* TODO: Check for duplicate labels */
			this->add_symbol_here(token.value);
			} break;
		case TK_OPCODE: {
				align(4);
				auto il = token.opcode->handler(*this);
				for (auto instr : il) {
					output.insert(output.end(), instr.raw, instr.raw + instr.size());
				}
			} break;
		case TK_STRING:
		case TK_SYMBOL:
		case TK_REGISTER:
		case TK_CONSTANT:
			fprintf(stderr, "Unexpected token %s at line %u\n",
				token.to_string().c_str(), token.line);
			throw std::runtime_error("Unexpected token: " + token.to_string());
		}
	}

	this->finish_scheduled_work();
}

Instruction& Assembler::instruction_at(address_t addr)
{
	return *(Instruction*)&output.at(addr - options.base);
}

Instruction& Assembler::instruction_at_offset(uint64_t off)
{
	return *(Instruction*)&output.at(off);
}

bool Assembler::symbol_is_known(const Token& tk) const
{
	return lookup.find(tk.value) != lookup.end();
}
void Assembler::add_symbol_here(const std::string& name) {
	lookup.emplace(std::piecewise_construct,
		std::forward_as_tuple(name),
		std::forward_as_tuple(current_address()));
}
address_t Assembler::address_of(const std::string& name) const
{
	auto it = lookup.find(name);
	if (it != lookup.end()) return it->second;

	throw std::runtime_error("No such symbol: " + name);
}
address_t Assembler::address_of(const Token& tk) const
{
	auto it = lookup.find(tk.value);
	if (it != lookup.end()) return it->second;

	token_exception(tk, "resolve symbol");
}

void Assembler::schedule(const Token& tk, scheduled_op_t op)
{
	const auto& sym = tk.value;
	m_schedule[sym].push_back(std::move(op));
}
void Assembler::finish_scheduled_work()
{
	for (auto& it : m_schedule) {
		const auto address = address_of(it.first);
		for (auto& op : it.second)
			op(*this, address);
	}
}

void Assembler::token_exception(const Token& tk, const std::string& info) const
{
	fprintf(stderr, "Wrong token on line %u. Info: %s\n", tk.line, info.c_str());
	throw std::runtime_error("Wrong token type: " + tk.to_string());

}
