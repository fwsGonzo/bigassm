#include "assembler.hpp"
#include <map>
extern bool file_writer(const std::string&, const std::vector<uint8_t>&);
static constexpr bool VERBOSE_SECTIONS = true;

struct ElfStringSection {
	Elf_Shdr& shdr;
	std::vector<uint8_t>& bin;
	std::map<std::string, size_t> namelist;
	const int shindex;
	size_t offset = 0;

	ElfStringSection(Elf_Shdr& sh, int idx, std::vector<uint8_t>& b)
		: shdr{sh}, bin{b}, shindex{idx}
	{
		shdr.sh_name = 0;
		shdr.sh_type = SHT_STRTAB;
		shdr.sh_flags = 0;
		shdr.sh_entsize = 1;
		shdr.sh_offset = bin.size();
		shdr.sh_size = 0;
		shdr.sh_addralign = 1;
		shdr.sh_info = 0;
		shdr.sh_link = 0;
		this->append_null();
	}

	size_t add(const std::string& str) {
		bin.insert(bin.end(), str.begin(), str.end()+1);
		shdr.sh_size += str.size()+1;
		const size_t offset = this->offset;
		namelist[str] = offset;
		this->offset += str.size()+1;
		return offset;
	}
	size_t lookup(const std::string& str) const {
		return namelist.at(str);
	}
	void append_null() {
		bin.push_back(0);
		shdr.sh_size += 1;
		this->offset += 1;
	}
};

struct ElfSymSection {
	Elf_Shdr& shdr;
	std::vector<uint8_t>& bin;

	ElfSymSection(Elf_Shdr& sh, int strindex, std::vector<uint8_t>& b)
		: shdr{sh}, bin{b}
	{
		shdr.sh_type = SHT_SYMTAB;
		shdr.sh_flags = 0;
		shdr.sh_entsize = sizeof(Elf_Sym);
		shdr.sh_offset = bin.size();
		shdr.sh_size = 0;
		shdr.sh_info = 0;
		shdr.sh_addralign = alignof(Elf_Addr);
		shdr.sh_link = strindex;
		Elf_Sym nosym {};
		this->add_sym(nosym);
	}

	void add(const std::string& name, Elf_Addr addr, int info, size_t size, ElfStringSection& str) {
		Elf_Sym sym {};
		sym.st_name = str.lookup(name);
		sym.st_shndx = SHN_UNDEF;
		sym.st_info  = info;
		sym.st_other = STV_DEFAULT;
		sym.st_value = addr;
		sym.st_size  = size;
		this->add_sym(sym);
	}
	void add_sym(const Elf_Sym& sym) {
		const uint8_t* s = (const uint8_t *)&sym;
		bin.insert(bin.end(), s, s + sizeof(sym));
		shdr.sh_size += sizeof(sym);
		shdr.sh_info ++;
	}

	size_t count() const noexcept {
		return shdr.sh_info;
	}
};

struct ElfData {
	static inline constexpr size_t S = 3;
	Elf_Ehdr hdr;
	Elf_Shdr shdr[1 + S];
	Elf_Phdr phdr[0];
};

void Elf_Writer(const Options& options,
	Assembler& assembler, const std::string& outfile)
{
	auto& sections = assembler.sections();
	size_t section_data_size = 0;
	for (const auto& it : sections) {
		auto& section = it.second;
		if constexpr (VERBOSE_SECTIONS) {
			printf("Section %s has: CODE=%d DATA=%d RESV=%d\n",
				section.name().c_str(), section.code, section.data, section.resv);
		}
		if (section.code || section.data)
			section_data_size += section.size();
	}

	std::vector<uint8_t> elfbin;
	const size_t elfhdr_size = sizeof(ElfData) +
		sizeof(Elf_Phdr) * sections.size();
	elfbin.reserve(262144);
	elfbin.resize(elfhdr_size);
	auto* elfdata = (ElfData*) elfbin.data();
	auto& elf = elfdata->hdr;
	elf.e_ident[EI_MAG0] = ELFMAG0;
	elf.e_ident[EI_MAG1] = ELFMAG1;
	elf.e_ident[EI_MAG2] = ELFMAG2;
	elf.e_ident[EI_MAG3] = ELFMAG3;
	elf.e_ident[EI_CLASS] = ELFCLASS_XX;
	elf.e_ident[EI_DATA] = ELFDATA2LSB;
	elf.e_ident[EI_VERSION] = EV_CURRENT;
	elf.e_ident[EI_OSABI] = ELFOSABI_STANDALONE;
	elf.e_type = ET_EXEC;
	elf.e_machine = EM_RISCV;
	elf.e_version = EV_CURRENT;
	elf.e_entry = assembler.address_of(options.entry);
	elf.e_phoff = offsetof(ElfData, phdr);
	elf.e_shoff = offsetof(ElfData, shdr);
	elf.e_ehsize = sizeof(Elf_Ehdr);
	elf.e_phentsize = sizeof(Elf_Phdr);
	elf.e_phnum = sections.size();
	elf.e_shentsize = sizeof(Elf_Shdr);
	elf.e_shnum = 1 + ElfData::S;
	elf.e_shstrndx = 1;

	ElfStringSection shnames { elfdata->shdr[1], 1, elfbin };
	/* We need to all all sections now. */
	shnames.shdr.sh_name = shnames.add(".shstrtab");
	shnames.add(".symtab");
	shnames.add(".strtab");

	/* Add all strings now. */
	ElfStringSection strings { elfdata->shdr[3], 3, elfbin };
	strings.shdr.sh_name = shnames.lookup(".strtab");
	for (const auto& sit : assembler.symbols()) {
		strings.add(sit.first);
	}

	/* Add all the visible symbols */
	ElfSymSection syms { elfdata->shdr[2], strings.shindex, elfbin };
	syms.shdr.sh_name = shnames.lookup(".symtab");
	for (const auto& sit : assembler.symbols()) {
		auto& sym = sit.second;
		uint32_t info = sym.type | SHN_ABS;
		if (assembler.globals().count(sit.first) > 0)
			info |= STB_GLOBAL;
		else {
			info |= STB_LOCAL;
		}
		syms.add(sit.first, sym.address(), sym.type, sym.size, strings);
	}
	syms.shdr.sh_info = assembler.symbols().size()+1;
	file_writer(outfile, elfbin);

	size_t sect = 0;
	for (const auto& it : sections)
	{
		auto& program = elfdata->phdr[sect++];
		auto& section = it.second;
		const bool loadable = section.code || section.data;
		program.p_type = loadable ? PT_LOAD : 0x0;
		program.p_flags = 0;
		if (section.code) {
			program.p_flags |= PF_X;
			if (section.data || section.resv)
				fprintf(stderr, "WARNING: There is data in executable section %s\n",
					section.name().c_str());
		}
		if (!section.code || !section.execonly) {
			if (section.data || section.resv) {
				program.p_flags |= PF_R;
				if (!section.readonly) program.p_flags |= PF_W;
			}
		}
		if constexpr (VERBOSE_SECTIONS) {
		printf("ELF program header %s flags %c%c%c\n",
			section.name().c_str(),
			(program.p_flags & PF_R) ? 'R' : ' ',
			(program.p_flags & PF_W) ? 'W' : ' ',
			(program.p_flags & PF_X) ? 'X' : ' ');
		}
		program.p_offset = elfbin.size();
		program.p_vaddr = section.base_address();
		program.p_paddr = section.base_address();
		program.p_filesz = (loadable) ? section.size() : 0u;
		program.p_memsz = section.size();
		program.p_align = 0x0;
		elfbin.insert(elfbin.end(), section.output.begin(), section.output.end());
	}
	file_writer(outfile, elfbin);
}
