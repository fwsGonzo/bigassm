#include "assembler.hpp"
#include <sstream>

#define FLUSH_WORD() \
	if (!word.empty()) {	\
		tokens.push_back({word, line});	\
		word.clear();	\
	}

std::vector<RawToken> Assembler::split(const std::string& s)
{
	std::vector<RawToken> tokens;

	std::string word;
	uint32_t line = 1;
	bool begin_quotes = false;
	bool begin_comment = false;
	for (char c : s)
	{
		if (begin_quotes) {
			word.append(1, c);
			if (c == '"') {
				tokens.push_back({word, line});
				word.clear();
				begin_quotes = false;
			}
			continue;
		}
		if (begin_comment) {
			if (c == '\n') {
				begin_comment = false;
			}
			continue;
		}
		switch (c) {
		case ';':
			begin_comment = true;
			continue;
		case '+':
		case '-':
		case '*':
			/* This allows building constant chains */
			FLUSH_WORD();
			word.append(1, c);
			break;
		case ',':
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			FLUSH_WORD();
			if (c == '\n') line++;
			break;
		case '"':
			begin_quotes = true;
			[[fallthrough]];
		default:
			word.append(1, c);
		}
	}
	if (!word.empty())
		tokens.push_back({word, line});

    return tokens;
}
