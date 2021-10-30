#include "types.hpp"
#include <cstring>

Token::Token(const Token& other)
	: type(other.type), line(other.line), value(other.value)
{
	switch (type) {
	case TK_OPCODE:
		opcode = other.opcode; break;
	case TK_PSEUDOOP:
		pseudoop = other.pseudoop; break;
	default:
		std::memcpy(raw, other.raw, sizeof(raw));
	}
}

std::string Token::to_string() const
{
	return to_string(this->type) + " " + value;
}

std::string Token::to_string(TokenType tt)
{
	switch (tt) {
	case TK_DIRECTIVE:
		return "[Directive]";
	case TK_LABEL:
		return "[Label]";
	case TK_STRING:
		return "[String]";
	case TK_OPCODE:
		return "[Opcode]";
	case TK_REGISTER:
		return "[Register]";
	case TK_SYMBOL:
		return "[Symbol]";
	case TK_CONSTANT:
		return "[Constant]";
	default:
		return "Unknown token: " + std::to_string(tt);
	}
}
