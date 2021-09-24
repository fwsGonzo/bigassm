#include "assembler.hpp"
extern std::string load_file(const std::string&);

void Assembler::directive(const Token& token)
{
	if (token.value == ".align") {
		this->align(next<TK_CONSTANT>().u64);
	} else if (token.value == ".finish_labels") {
		this->align_with_labels(0);
	} else if (token.value == ".global") {
		const auto& sym = next<TK_SYMBOL>();
		this->make_global(sym.value);
	} else if (token.value == ".include") {
		const auto& sym = next<TK_STRING>();
		auto contents = load_file(sym.value);
		auto raw_tokens = Assembler::split(contents);
		auto tokens = Assembler::parse(raw_tokens);
		this->assemble(tokens);
	} else if (token.value == ".section") {
		/* Sections aren't really directives, but they do
		   start with a . (dot), so use that for simplicity. */
		const auto& section = next<TK_DIRECTIVE>();
		this->set_section(section.value);
	} else if (token.value == ".org") {
		const auto& ba = next<TK_CONSTANT>();
		if (current_section().size() > 0)
			throw std::runtime_error("Cannot change base address when instructions already generated");
		current_section().set_base_address(ba.u128);
		printf("Base address: 0x%s\n",
			to_hex_string(current_section().base_address()).c_str());
	} else if (token.value == ".execonly") {
		current_section().make_execonly();
	} else if (token.value == ".readonly") {
		current_section().make_readonly();
	} else if (token.value == ".type") {
		const auto& sym = next<TK_SYMBOL>();
		const auto& cls = next<TK_SYMBOL>();
		(void) sym;
		(void) cls;
	} else if (token.value == ".size") {
		const auto& sym = next<TK_SYMBOL>();
		const uint32_t size = 0;
		auto dataloc = current_location();
		this->align_with_labels(alignof(decltype(size)));
		auto loc = current_location();
		const auto* src = (const uint8_t *)&size;

		this->schedule(sym,
		[loc, dataloc] (Assembler& a, address_t addr) {
			auto& size = a.at_location<uint32_t>(loc);
			if (addr < dataloc.address())
				size = dataloc.address() - addr; /* NB: Opposite */
			else
				size = addr - 4 - loc.address(); /* NB: Forward */
		});
		add_output(OT_DATA, src, sizeof(size));

	} else if (token.value == ".string") {
		const auto& str = next<TK_STRING>();
		this->align_with_labels(0);
		/* We want the zero-termination */
		add_output(OT_DATA, str.value.data(), str.value.size()+1);
	} else {
		fprintf(stderr, "Unknown directive: %s\n", token.value.c_str());
	}
}
