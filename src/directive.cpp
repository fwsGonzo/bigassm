#include "assembler.hpp"
#include <elf.h>
extern std::string load_file(const std::string&, const char*);
extern const char* get_realpath(const char* path);

void Assembler::directive(const Token& token)
{
	if (token.value == ".align") {
		this->align(next<TK_CONSTANT>().u64);
	} else if (token.value == ".endfunc") {
		const auto& sym = next<TK_SYMBOL>();
		auto loc = current_location();

		this->schedule(sym,
		[loc] (Assembler&, const std::string&, auto& sym) {
			sym.size = loc.address() - sym.address();
			sym.type = STT_FUNC;
		});
	} else if (token.value == ".finish_labels") {
		this->align_with_labels(0);
	} else if (token.value == ".global") {
		const auto& sym = next<TK_SYMBOL>();
		this->make_global(sym.value);
	} else if (token.value == ".include") {
		const auto& sym = next<TK_STRING>();
		auto contents = load_file(sym.value, m_realpath);
		auto raw_tokens = Assembler::split(contents);
		auto tokens = Assembler::parse(raw_tokens);
		const char* rpath = get_realpath(sym.value.c_str());
		this->assemble(tokens, rpath);
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
		const auto& info = next<TK_SYMBOL>();
		if (info.value == "object") {
			this->symbol_set_type(sym.value, STT_OBJECT);
		} else if (info.value == "func" || info.value == "function") {
			this->symbol_set_type(sym.value, STT_FUNC);
		} else {
			throw std::runtime_error("Unknown type: " + info.value);
		}
	} else if (token.value == ".size") {
		const auto& sym = next<TK_SYMBOL>();
		const uint32_t size = 0;
		auto dataloc = current_location();
		this->align_with_labels(alignof(decltype(size)));
		auto loc = current_location();
		const auto* src = (const uint8_t *)&size;

		this->schedule(sym,
		[loc, dataloc] (Assembler& a, const std::string&, auto& sym) {
			auto& size = a.at_location<uint32_t>(loc);
			if (sym.address() < dataloc.address())
				size = dataloc.address() - sym.address(); /* NB: Opposite */
			else
				size = sym.address() - 4 - loc.address(); /* NB: Forward */
			sym.size = size;
		});
		/* NOTE: We add a length of zero here, and fix
		   the actual output later on when length is known. */
		add_output(OT_DATA, src, sizeof(size));

	} else if (token.value == ".string") {
		const auto& str = next<TK_STRING>();
		this->align_with_labels(0);
		/* We want the zero-termination */
		add_output(OT_DATA, str.value.data(), str.value.size()+1);
	} else if (token.value == ".strlen") {
		const auto& str = next<TK_STRING>();
		const uint32_t size = str.value.size();
		this->align_with_labels(alignof(decltype(size)));
		add_output(OT_DATA, &size, sizeof(size));
	} else if (token.value == ".ascii") {
		const auto& str = next<TK_STRING>();
		this->align_with_labels(0);
		add_output(OT_DATA, str.value.data(), str.value.size());
	} else {
		fprintf(stderr, "Unknown directive: %s\n", token.value.c_str());
	}
}
