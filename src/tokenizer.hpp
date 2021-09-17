#pragma once
#include "types.hpp"
#include <vector>

struct Tokenizer
{
	static std::vector<Token>
		parse(const std::vector<RawToken>& words);
};
