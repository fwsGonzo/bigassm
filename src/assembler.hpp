#pragma once
#include "section.hpp"
#include <functional>
#include <map>
#include <unordered_map>
#include <set>
#include <stdexcept>

struct Options {
	address_t base = 0x100000;
	std::string entry = "_start";
	bool section_attr_page_separation = true;
	bool verbose_labels = false;
};

struct Assembler
{
	using scheduled_op_t = std::function<void(Assembler&, const std::string&, SymbolLocation&)>;

	static std::vector<Token> split(const std::string&);
	static Token parse(const RawToken&);

	void assemble(const std::vector<Token>&, const char* rpath);
	void finish();

	const Token& next() {
		return tokens->at(index++);
	}
	template <TokenType T>
	const Token& next(const std::string& info = "") {
		const auto& tk = next();
		if (tk.type != T) {
			argument_mismatch(tk, T, info);
		}
		return tk;
	}
	bool next_is(TokenType tt) const {
		if (done()) return false;
		return tokens->at(index).type == tt;
	}
	Token resolve_constants();
	bool done() const noexcept { return index >= tokens->size(); }

	bool is_aligned(size_t alignment) {
		return (current_section().size() & (alignment-1)) == 0;
	}
	void align(size_t alignment) { current_section().align(alignment); }
	void align_with_labels(size_t alignment);

	void add_output(OutputType, const void* vdata, size_t len);
	void allocate(size_t len);

	auto& sections() noexcept { return m_sections; }
	void set_section(const std::string&);
	Section& section(const std::string&);
	Section& current_section() noexcept { return *m_current_section; }
	const Section& current_section() const noexcept { return *m_current_section; }
	SymbolLocation current_location() const noexcept { return m_current_section->current_location(); }

	bool symbol_is_known(const Token&) const;
	address_t address_of(const Token&) const;
	address_t address_of(const std::string&) const;
	void schedule(const std::string&, scheduled_op_t);
	void schedule(const Token&, scheduled_op_t);

	void directive(const Token&);
	void add_symbol_here(const std::string& name);
	const auto& symbols() const noexcept { return m_lookup; }
	void make_global(const std::string& name);
	void symbol_set_type(const std::string& name, uint32_t type);
	const auto& globals() const noexcept { return m_globals; }
	void add_label_soon(const std::string& name);

	template <typename T>
	T& at_location(SymbolLocation, size_t off = 0);
	Instruction& instruction_at(SymbolLocation, size_t off = 0);

	[[noreturn]] void token_exception(const Token&, const std::string&) const;
	[[noreturn]] void argument_mismatch(const Token&, TokenType, const std::string&) const;

	Assembler(const Options& opt);
	const Options& options;
	const char* realpath() const noexcept { return m_realpath; }
private:
	void resolve_base_addresses();
	void finish_scheduled_work();

	const std::vector<Token>* tokens = nullptr;
	size_t index = 0;

	Section* m_current_section = nullptr;
	std::map<std::string, Section> m_sections;
	std::unordered_map<std::string, SymbolLocation> m_lookup;
	std::map<std::string, std::vector<scheduled_op_t>> m_schedule;
	std::set<std::string> m_globals;
	const char* m_realpath;
};

template <typename T>
inline T& Assembler::at_location(SymbolLocation loc, size_t off) {
	auto& section = *loc.section;
	auto& output = section.output;
	return *(T*)&output.at(loc.address() + off - section.base_address());
}
