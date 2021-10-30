#pragma once
#include "types.hpp"

struct Registers {
	static Token to_reg(const std::string&);
	static void print_all();
};
