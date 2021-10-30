#include "assembler.hpp"
#include <sstream>

#define FLUSH_WORD() \
	if (!word.empty()) {	\
		tokens.push_back({word, line});	\
		word.clear();	\
	}

static char escape_character(char echar)
{
	switch (echar) {
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'b': return '\b';
	case '\\': return '\\';
	case '\'': return '\'';
	case '\"': return '\"';
	case '0': return 0;
	default:
		fprintf(stderr, "Unknown escape character: %c\n", echar);
		return echar;
	}
}

std::vector<RawToken> Assembler::split(const std::string& s)
{
	std::vector<RawToken> tokens;

	std::string word;
	uint32_t line = 1;
	bool begin_quotes = false;
	bool begin_comment = false;
	bool begin_escape = false;
	for (char c : s)
	{
		bool is_escaped = false;
		if (begin_escape) {
			c = escape_character(c);
			begin_escape = false;
			is_escaped = true;
		}
		if (begin_quotes) {
			if (c == '\\') {
				begin_escape = true;
				continue;
			}
			word.append(1, c);
			if (c == '"' && !is_escaped) {
				tokens.push_back({word, line});
				word.clear();
				begin_quotes = false;
			}
			continue;
		}
		else if (begin_comment) {
			if (c == '\n') {
				begin_comment = false;
			}
			continue;
		}
		switch (c) {
		case ';':
			begin_comment = true;
			continue;
		case '\\':
			begin_escape = true;
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
