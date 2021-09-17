#include "opcodes.hpp"
#include "opcode_list.cpp"

Token Opcodes::opcode(const std::string& value)
{
	Token tk;
	tk.value = value;

	auto it = opcode_list.find(value);
	if (it != opcode_list.end())
	{
		tk.type = TK_OPCODE;
		tk.opcode = &it->second;
		return tk;
	}
	tk.type = TK_SYMBOL;
	return tk;
}
