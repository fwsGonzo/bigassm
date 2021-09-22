#include "assembler.hpp"

void Assembler::directive(const Token& token)
{
	if (token.value == ".align") {
		align(next<TK_CONSTANT>().u64);
	} else if (token.value == ".global") {
		const auto& sym = next<TK_SYMBOL>();
		(void) sym;
	} else if (token.value == ".org") {
		const auto& ba = next<TK_CONSTANT>();
		if (!output.empty())
			throw std::runtime_error("Cannot change base address when instructions already generated");
		this->m_base_address = ba.u128;
		printf("Base address: 0x%s\n", to_hex_string(m_base_address).c_str());
	} else if (token.value == ".type") {
		const auto& sym = next<TK_SYMBOL>();
		const auto& cls = next<TK_SYMBOL>();
		(void) sym;
		(void) cls;
	} else if (token.value == ".size") {
		const auto& sym = next<TK_SYMBOL>();
		const uint32_t size = current_address() - address_of(sym.value);
		this->align(alignof(decltype(size)));
		const auto* src = (const uint8_t *)&size;
		output.insert(output.end(), src, src + sizeof(size));
	} else if (token.value == ".string") {
		const auto& str = next<TK_STRING>();
		this->align_with_labels(0);
		/* We want the zero-termination */
		output.insert(output.end(),
			str.value.data(), str.value.data() + str.value.size()+1);
	} else {
		fprintf(stderr, "Unknown directive: %s\n", token.value.c_str());
	}
}
