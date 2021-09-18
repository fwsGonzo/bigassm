#include <functional>
struct Assembler;

struct PseudoOp
{
	std::function<void(Assembler&)> handler;
};
