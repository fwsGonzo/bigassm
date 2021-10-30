#include "assembler.hpp"
#include "opcodes.hpp"
#include "pseudo_ops.hpp"
#include "registers.hpp"
#include <cassert>

Assembler::Assembler(const Options& opt)
	: options(opt)
{
	this->set_section(".text"); /* Default section */
	current_section().set_base_address(options.base);
}

void Assembler::assemble(const std::vector<Token>& tv,
	const char* rpath)
{
	const auto prev_tokens = this->tokens;
	const auto prev_index = this->index;
	const auto prev_rpath = this->m_realpath;
	this->tokens = &tv;
	this->index = 0;
	this->m_realpath = rpath;
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
		case TK_OPERATOR:
		case TK_UNSPEC:
			fprintf(stderr, "Unexpected token %s at line %u\n",
				token.to_string().c_str(), token.line);
			throw std::runtime_error("Unexpected token: " + token.to_string());
		}
	}
	/* Restore any previous assembler operation. */
	this->tokens = prev_tokens;
	this->index = prev_index;
	this->m_realpath = prev_rpath;
}
void Assembler::finish()
{
	/* Output any remaining unattached labels. */
	for (auto& sit : sections()) {
		sit.second.align_with_labels(*this, 1);
	}
	/* Calculate addresses for each section in the
	   order they appear, unless the section has
	   a custom base address. */
	this->resolve_base_addresses();
	/* Resolve addresses, sizes, custom symbol data. */
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
	return m_lookup.find(tk.value) != m_lookup.end();
}
void Assembler::add_symbol_here(const std::string& name) {
	m_lookup.emplace(std::piecewise_construct,
		std::forward_as_tuple(name),
		std::forward_as_tuple(
			SymbolLocation{m_current_section, m_current_section->size()}));
}
address_t Assembler::address_of(const std::string& name) const
{
	auto it = m_lookup.find(name);
	if (it != m_lookup.end())
		return it->second.address();

	throw std::runtime_error("No such symbol: " + name);
}
address_t Assembler::address_of(const Token& tk) const
{
	auto it = m_lookup.find(tk.value);
	if (it != m_lookup.end())
		return it->second.address();

	token_exception(tk, "resolve symbol");
}

void Assembler::schedule(const Token& tk, scheduled_op_t op)
{
	const auto& sym = tk.value;
	m_schedule[sym].push_back(std::move(op));
}
void Assembler::schedule(const std::string& sym, scheduled_op_t op)
{
	m_schedule[sym].push_back(std::move(op));
}
void Assembler::finish_scheduled_work()
{
	for (auto& it : m_schedule) {
		const auto& name = it.first;
		auto sit = m_lookup.find(name);
		if (sit != m_lookup.end()) {
			for (auto& op : it.second)
				op(*this, sit->first, sit->second);
		} else {
			throw std::runtime_error("Unknown symbol scheduled: " + name);
		}
	}
}

void Assembler::symbol_set_type(const std::string& name, uint32_t type)
{
	schedule(name,
	[type] (Assembler&, auto&, SymbolLocation& sym) {
		sym.type = type;
	});
}
void Assembler::make_global(const std::string& name)
{
	m_globals.insert(name);
}

Token Assembler::resolve_constants()
{
	auto& imm = this->next <TK_CONSTANT> ();
	Token sum {imm};
	enum {
		NONE,
		ADD,
		SUB,
		MUL,
		DIV,
		SHR,
		SHL,
		MOD,
		AND,
		OR,
		XOR,
	} ops {NONE};

	bool is_negated = false;

	while (next_is(TK_CONSTANT) || next_is(TK_OPERATOR)) {
		if (next_is(TK_CONSTANT)) {
			auto& tk = next <TK_CONSTANT> ();

			auto value = tk.u128;
			if (is_negated) {
				is_negated = false;
				value = ~value;
			}

			switch (ops) {
			case ADD:
				sum.u128 = sum.u128 + value;
				break;
			case SUB:
				sum.u128 = sum.u128 - value;
				break;
			case MUL:
				sum.u128 = sum.u128 * value;
				break;
			case DIV:
				sum.u128 = sum.u128 / value;
				break;
			case SHR:
				sum.u128 = sum.u128 >> value;
				break;
			case SHL:
				sum.u128 = sum.u128 << value;
				break;
			case MOD:
				sum.u128 = sum.u128 % value;
				break;
			case AND:
				sum.u128 = sum.u128 & value;
				break;
			case OR:
				sum.u128 = sum.u128 | value;
				break;
			case XOR:
				sum.u128 = sum.u128 ^ value;
				break;
			case NONE:
			default:
				token_exception(tk, "Unexpected constant");
				break;
			}
		} else if (next_is(TK_OPERATOR)) {
			auto& tk = next <TK_OPERATOR> ();
			if (tk.value == "~") {
				is_negated = !is_negated;
			} else {
				if (ops != NONE)
					token_exception(tk, "Unexpected operator");
				if (tk.value == "+")
					ops = ADD;
				else if (tk.value == "-")
					ops = SUB;
				else if (tk.value == "*")
					ops = MUL;
				else if (tk.value == "/")
					ops = DIV;
				else if (tk.value == ">>")
					ops = SHR;
				else if (tk.value == "<<")
					ops = SHL;
				else if (tk.value == "%")
					ops = MOD;
				else if (tk.value == "&")
					ops = AND;
				else if (tk.value == "|")
					ops = OR;
				else if (tk.value == "^")
					ops = XOR;
				else token_exception(tk,
					"Unknown operator: " + tk.value);
			}
		}
	}

	return sum;
}
void Assembler::token_exception(const Token& tk, const std::string& info) const
{
	fprintf(stderr, "*** Problem on line %u: %s\n", tk.line, info.c_str());
	throw std::runtime_error("Token: " + tk.to_string());
}
void Assembler::argument_mismatch(const Token& tk, TokenType T, const std::string& info) const
{
	if (T == TK_REGISTER) {
		Registers::print_all();
	}
	token_exception(tk,
		"Argument mismatch. Expected " + Token::to_string(T)
		+ ", found " + Token::to_string(tk.type) + " instead."
		+ info);
}
