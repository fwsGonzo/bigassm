#include "tokenizer.hpp"
#include "opcodes.hpp"
#include "registers.hpp"
#include <cassert>
#include <stdexcept>

static bool is_number(char c) {
	return (c == '-' || c == '+' || (c >= '0' && c <= '9'));
}

std::vector<Token>
Tokenizer::parse(const std::vector<RawToken>& raw_tokens)
{
	std::vector<Token> tokens;
	for (const auto& rt : raw_tokens)
	{
		const auto& word = rt.name;
		assert(!word.empty());
		Token tk;
		tk.type = TK_SYMBOL;
		tk.line = rt.line;
		if (word[0] == '.') {
			tk.type = TK_DIRECTIVE;
			tk.value = word;
		} else if (word.back() == ':') {
			tk.type = TK_LABEL;
			tk.value = word.substr(0, word.size() - 1);
		} else if (word[0] == '"') {
			tk.type = TK_STRING;
			tk.value = word.substr(1, word.size() - 2);
		} else if (is_number(word[0])) {
			if (word.size() > 2 && word[1] == 'x') {
				tk.u64 = std::stoul(&word[2], nullptr, 16);
			} else if (word.size() > 2 && word[1] == 'b') {
				tk.u64 = std::stoul(&word[2], nullptr, 2);
			} else { // base 10
				tk.i64 = atoi(word.c_str());
			}
			tk.type = TK_CONSTANT;
			tk.value = word;
		} else {
			tk = Opcodes::opcode(word);
			// Try again with register
			if (tk.is_symbol())
				tk = Registers::to_reg(word);
		}
		tokens.push_back(tk);
	}
	return tokens;
}
