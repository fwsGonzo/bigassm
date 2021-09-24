#pragma once
#include "types.hpp"

enum OutputType {
	OT_CODE = 0,
	OT_DATA = 1,
	OT_RESV = 2,
};

struct SymbolLocation {
	const Section* section;
	uint64_t offset;

	address_t address() const noexcept;
};

struct Section {
	address_t base_address() const noexcept { return m_base_address; }
	bool has_base_address() const noexcept { return m_has_base_addr; }
	void set_base_address(address_t nba) noexcept;
	address_t current_address() const noexcept { return base_address() + size(); }
	SymbolLocation current_location() const noexcept { return {this, size()}; }

	address_t address_at(uint64_t offset) const noexcept { return m_base_address + offset; }

	void add_output(OutputType, const void* vdata, size_t len);
	void allocate(size_t len);
	void align(size_t alignment);
	void align_with_labels(Assembler&, size_t alignment);
	void make_readonly() { this->readonly = true; }

	const std::string& name() const noexcept { return m_name; }
	int index() const noexcept { return m_idx; }
	size_t size() const noexcept { return output.size(); }

	void add_label_soon(const std::string& name);
	void add_label_here(Assembler&, const std::string& name);

	std::vector<uint8_t> output;
	bool code = false;
	bool data = false;
	bool resv = false;
	bool readonly = false;

	Section(const std::string& name, int idx) : m_name{name}, m_idx{idx} {}
private:
	std::string m_name;
	const int   m_idx;
	bool m_has_base_addr = false;
	std::vector<std::string> m_label_queue;
	address_t m_base_address = 0;
};

inline address_t SymbolLocation::address() const noexcept {
	return section->address_at(offset);
}
