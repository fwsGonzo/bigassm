#pragma once
#include "types.hpp"
#include <functional>
#include <map>
#include <unordered_map>
#include <stdexcept>

struct Options {
	address_t base = 0x100000;
};

struct Assembler
{
	using scheduled_op_t = std::function<void(Assembler&, address_t)>;

	void assemble();
	void directive(const Token&);

	const Token& next() {
		return tokens.at(index++);
	}
	template <TokenType T>
	const Token& next() {
		const auto& tk = next();
		if (tk.type != T) {
			token_exception(tk, "argument");
		}
		return tk;
	}
	bool needs(size_t args) const noexcept { return index + args <= tokens.size(); }
	bool done() const noexcept { return index >= tokens.size(); }

	address_t base_address() const noexcept { return options.base; }
	address_t current_address() const noexcept { return base_address() + output.size(); }
	bool is_aligned(size_t alignment) {
		return (output.size() & (alignment-1)) == 0;
	}
	void align(size_t alignment) {
		size_t newsize = (output.size() + (alignment-1)) & ~(alignment-1);
		if (output.size() != newsize)
			output.resize(newsize);
	}

	bool symbol_is_known(const Token&) const;
	address_t address_of(const Token&) const;
	void schedule(const Token&, scheduled_op_t);

	void add_symbol_here(const std::string& name);

	Instruction& instruction_at(address_t);

	[[noreturn]] void token_exception(const Token&, const std::string&) const;

	Assembler(const Options& opt, const std::vector<Token>& t, std::vector<uint8_t>& out)
		: options(opt), tokens(t), output(out) {}

	const Options& options;
	const std::vector<Token>& tokens;
	size_t index = 0;
	std::vector<uint8_t>& output;

private:
	std::unordered_map<std::string, address_t> lookup;
	std::map<std::string, std::vector<scheduled_op_t>> m_schedule;
};
