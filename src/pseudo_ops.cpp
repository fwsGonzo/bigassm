#include "pseudo_ops.hpp"

#include "assembler.hpp"
#include <unordered_map>
extern std::string load_file(const std::string&);

static PseudoOp DATA_128 {
	.handler = [] (Assembler& a) {
		auto& constant = a.next<TK_CONSTANT> ();
		__uint128_t value = constant.u64;
		a.align_with_labels(16);
		a.add_output(&value, sizeof(value));
	}
};
static PseudoOp DATA_64 {
	.handler = [] (Assembler& a) {
		auto& constant = a.next<TK_CONSTANT> ();
		a.align_with_labels(8);
		a.add_output(&constant.u64, sizeof(uint64_t));
	}
};
static PseudoOp DATA_32 {
	.handler = [] (Assembler& a) {
		auto& constant = a.next<TK_CONSTANT> ();
		a.align_with_labels(4);
		uint32_t value = constant.u64;
		a.add_output(&value, sizeof(value));
	}
};
static PseudoOp DATA_16 {
	.handler = [] (Assembler& a) {
		auto& constant = a.next<TK_CONSTANT> ();
		a.align_with_labels(2);
		uint16_t value = constant.u64;
		a.add_output(&value, sizeof(value));
	}
};
static PseudoOp DATA_8 {
	.handler = [] (Assembler& a) {
		auto& constant = a.next<TK_CONSTANT> ();
		uint8_t value = constant.u64;
		a.add_output(&value, sizeof(value));
	}
};

static PseudoOp RESV_8 {
	.handler = [] (Assembler& a) {
		auto& times = a.next<TK_CONSTANT> ();
		a.allocate(times.u64);
	}
};
static PseudoOp RESV_16 {
	.handler = [] (Assembler& a) {
		auto& times = a.next<TK_CONSTANT> ();
		a.align_with_labels(2);
		a.allocate(times.u64 * sizeof(uint16_t));
	}
};
static PseudoOp RESV_32 {
	.handler = [] (Assembler& a) {
		auto& times = a.next<TK_CONSTANT> ();
		a.align_with_labels(4);
		a.allocate(times.u64 * sizeof(uint32_t));
	}
};
static PseudoOp RESV_64 {
	.handler = [] (Assembler& a) {
		auto& times = a.next<TK_CONSTANT> ();
		a.align_with_labels(8);
		a.allocate(times.u64 * sizeof(uint64_t));
	}
};
static PseudoOp RESV_128 {
	.handler = [] (Assembler& a) {
		auto& times = a.next<TK_CONSTANT> ();
		a.align_with_labels(16);
		a.allocate(times.u64 * sizeof(__uint128_t));
	}
};

static PseudoOp INCBIN {
	.handler = [] (Assembler& a) {
		auto& filename = a.next<TK_STRING> ();
		auto contents = load_file(filename.value);
		a.add_output(contents.data(), contents.size());
	}
};

static const std::unordered_map<std::string, PseudoOp> pseudo_list =
{
	{"db",     DATA_8},
	{"dh",     DATA_16},
	{"dw",     DATA_32},
	{"dd",     DATA_64},
	{"dq",     DATA_128},

	{"resb",   RESV_8},
	{"resh",   RESV_16},
	{"resw",   RESV_32},
	{"resd",   RESV_64},
	{"resq",   RESV_128},

	{"incbin", INCBIN},
};

Token pseudo_op(const std::string& value)
{
	Token tk;
	tk.value = value;

	auto it = pseudo_list.find(value);
	if (it != pseudo_list.end())
	{
		tk.type = TK_PSEUDOOP;
		tk.pseudoop = &it->second;
		return tk;
	}
	tk.type = TK_SYMBOL;
	return tk;
}
