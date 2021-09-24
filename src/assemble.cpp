#include "assembler.hpp"
#include "opcodes.hpp"
#include "pseudo_ops.hpp"
#include <cassert>

Assembler::Assembler(const Options& opt)
	: options(opt)
{
	this->set_section(".text"); /* Default section */
	current_section().set_base_address(options.base);
}

void Assembler::assemble(const std::vector<Token>& tv)
{
	this->tokens = &tv;
	this->index = 0;
	assert((options.base & 0xF) == 0);

	while (!this->done())
	{
		auto& token = this->next();
		switch (token.type) {
		case TK_DIRECTIVE:
			this->directive(token);
			break;
		case TK_LABEL:
			current_section().add_label_soon(token.value);
			break;
		case TK_OPCODE: {
				align_with_labels(4);
				auto il = token.opcode->handler(*this);
				for (auto instr : il) {
					add_output(OT_CODE, instr.raw, instr.length());
				}
			} break;
		case TK_PSEUDOOP:
			token.pseudoop->handler(*this);
			break;
		case TK_STRING:
		case TK_SYMBOL:
		case TK_REGISTER:
		case TK_CONSTANT:
		case TK_UNSPEC:
			fprintf(stderr, "Unexpected token %s at line %u\n",
				token.to_string().c_str(), token.line);
			throw std::runtime_error("Unexpected token: " + token.to_string());
		}
	}
}
void Assembler::finish()
{
	this->resolve_base_addresses();
	this->finish_scheduled_work();
}

void Assembler::resolve_base_addresses()
{
	#define PAGE_REALIGN(base_addr) \
		base_addr = (base_addr + 0xFFF) & ~(address_t)0xFFF
	auto section_at = [this] (int idx) -> Section& {
		for (auto& it : m_sections) {
			if (it.second.index() == idx) return it.second;
		}
		throw std::runtime_error("Could not find section index " + std::to_string(idx));
	};
	address_t base_addr = options.base;
	bool was_executable = false;
	bool was_readonly = false;
	for (size_t idx = 0; idx < m_sections.size(); idx++)
	{
		auto& section = section_at(idx);
		if (!section.has_base_address()) {
			if (options.section_attr_page_separation) {
				/* Page-realign when going from executable to
				   non-executable, and readonly to writable. */
				if (was_executable && !section.code) {
					was_executable = false;
					PAGE_REALIGN(base_addr);
				} else if (was_readonly && !section.readonly) {
					was_readonly = false;
					PAGE_REALIGN(base_addr);
				}
			}
			section.set_base_address(base_addr);
		} else {
			base_addr = section.base_address();
			was_executable = section.code;
		}
		base_addr += section.size();
		/* XXX: Alignment? */
		base_addr = (base_addr + 0xF) & ~(address_t)0xF;
	}
}

Instruction& Assembler::instruction_at(SymbolLocation loc, size_t off) {
	return at_location<Instruction>(loc, off);
}

Section& Assembler::section(const std::string& name) {
	auto it = m_sections.find(name);
	if (it != m_sections.end()) return it->second;

	const int index = m_sections.size();
	m_sections.emplace(std::piecewise_construct,
		std::forward_as_tuple(name),
		std::forward_as_tuple(name, index));
	return m_sections.at(name);
}
void Assembler::set_section(const std::string& name) {
	auto& sect = this->section(name);
	this->m_current_section = &sect;
}

void Assembler::add_output(OutputType ot, const void* vdata, size_t len) {
	current_section().add_output(ot, vdata, len);
}
void Assembler::allocate(size_t len) {
	current_section().allocate(len);
}
void Assembler::align_with_labels(size_t alignment) {
	current_section().align_with_labels(*this, alignment);
}

bool Assembler::symbol_is_known(const Token& tk) const
{
	return lookup.find(tk.value) != lookup.end();
}
void Assembler::add_symbol_here(const std::string& name) {
	lookup.emplace(std::piecewise_construct,
		std::forward_as_tuple(name),
		std::forward_as_tuple(
			SymbolLocation{m_current_section, m_current_section->size()}));
}
address_t Assembler::address_of(const std::string& name) const
{
	auto it = lookup.find(name);
	if (it != lookup.end())
		return it->second.section->address_at(it->second.offset);

	throw std::runtime_error("No such symbol: " + name);
}
address_t Assembler::address_of(const Token& tk) const
{
	auto it = lookup.find(tk.value);
	if (it != lookup.end())
		return it->second.section->address_at(it->second.offset);

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

void Assembler::make_global(const std::string& name)
{
	m_globals.push_back(name);
}

void Assembler::token_exception(const Token& tk, const std::string& info) const
{
	fprintf(stderr, "Problematic token on line %u. Info: %s\n", tk.line, info.c_str());
	throw std::runtime_error("Token: " + tk.to_string());

}
