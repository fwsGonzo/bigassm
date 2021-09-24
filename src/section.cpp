#include "section.hpp"
#include "assembler.hpp"

void Section::set_base_address(address_t nba) noexcept {
	m_base_address = nba;
	m_has_base_addr = true;
}

void Section::add_label_soon(const std::string& name) {
	m_label_queue.push_back(name);
}
void Section::add_label_here(Assembler& a, const std::string& label) {
	if (a.options.verbose_labels) {
	printf("Label %s at %s off 0x%s\n",
		label.c_str(), name().c_str(), to_hex_string(current_address()).c_str());
	}
	/* TODO: Check for duplicate labels */
	a.add_symbol_here(label);
}

void Section::add_output(OutputType type, const void* vdata, size_t len) {
	const char* data = (const char*)vdata;
	output.insert(output.end(), data, data + len);
	if (type == OT_CODE) this->code = true;
	else if (type == OT_DATA) this->data = true;
	else if (type == OT_RESV) this->resv = true;
}
void Section::allocate(size_t len) {
	output.resize(output.size() + len);
	this->resv = true;
}
void Section::align(size_t alignment) {
	size_t newsize = (output.size() + (alignment-1)) & ~(alignment-1);
	if (output.size() != newsize)
		output.resize(newsize);
}
void Section::align_with_labels(Assembler& a, size_t alignment)
{
	if (alignment > 1)
		this->align(alignment);
	for (const auto& name : m_label_queue)
		add_label_here(a, name);
	m_label_queue.clear();
}
