#include "assembler.hpp"
#include "opcodes.hpp"
#include <cassert>

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
			printf("Label %s at address 0x%s\n",
				token.value.c_str(), to_hex_string(current_address()).c_str());
			/* TODO: Check for duplicate labels */
			this->add_symbol_here(token.value);
			} break;
		case TK_OPCODE: {
				align(4);
				auto il = token.opcode->handler(*this);
				for (auto instr : il) {
					output.insert(output.end(), instr.raw, instr.raw + instr.length());
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
