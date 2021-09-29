#include "assembler.hpp"
//#define OUTPUT_64BIT_ELF
#include "elf128.h"
#include <cstring>
#include <libgen.h>
extern std::string load_file(const std::string&, const char* = nullptr);
static bool file_writer(const std::string&, const std::vector<uint8_t>&);
extern std::vector<RawToken> split(const std::string&);
static constexpr bool VERBOSE_WORDS = false;
static constexpr bool VERBOSE_TOKENS = false;
static constexpr bool VERBOSE_SECTIONS = true;
static constexpr bool VERBOSE_GLOBALS = true;
static constexpr bool USE_ELF128 = true;

struct ElfData {
	static inline constexpr size_t S = 3;
	Elf128_Ehdr hdr;
	Elf128_Shdr shdr[1 + S];
	Elf128_Phdr phdr[0];
};

extern const char* get_realpath(const char* path) {
	/* XXX: Intentionally leaky. */
	char* copy = strdup(path);
	return realpath(dirname(copy), NULL);
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "%s [asm ...] [bin]\n", argv[0]);
		exit(1);
	}
	const std::string outfile = argv[argc-1];

	Options options;
	Assembler assembler(options);

	for (int i = 1; i < argc-1; i++)
	{
		const std::string infile = argv[1];
		auto input = load_file(infile);
		auto raw_tokens = Assembler::split(input);

		if constexpr (VERBOSE_WORDS) {
			for (const auto& rt : raw_tokens)
				printf("Word: %s\n", rt.name.c_str());
		}

		auto tokens = Assembler::parse(raw_tokens);
		if constexpr (VERBOSE_TOKENS) {
			for (auto& token : tokens)
				printf("Token: %s\n", token.to_string().c_str());
		}

		const char* rpath = get_realpath(infile.c_str());
		assembler.assemble(tokens, rpath);
	}

	assembler.finish();

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

	if constexpr (USE_ELF128) {
		std::vector<uint8_t> elfbin;
		const size_t elfhdr_size = sizeof(ElfData) +
			sizeof(Elf128_Phdr) * sections.size();
		elfbin.reserve(262144);
		elfbin.resize(elfhdr_size);
		auto* elfdata = (ElfData*) elfbin.data();
		auto& elf = elfdata->hdr;
		elf.e_ident[EI_MAG0] = ELFMAG0;
		elf.e_ident[EI_MAG1] = ELFMAG1;
		elf.e_ident[EI_MAG2] = ELFMAG2;
		elf.e_ident[EI_MAG3] = ELFMAG3;
		elf.e_ident[EI_CLASS] = ELFCLASS128;
		elf.e_ident[EI_DATA] = ELFDATA2LSB;
		elf.e_ident[EI_VERSION] = EV_CURRENT;
		elf.e_ident[EI_OSABI] = ELFOSABI_STANDALONE;
		elf.e_type = ET_EXEC;
		elf.e_machine = EM_RISCV;
		elf.e_version = EV_CURRENT;
		elf.e_entry = assembler.address_of(options.entry);
		elf.e_phoff = offsetof(ElfData, phdr);
		elf.e_shoff = offsetof(ElfData, shdr);
		elf.e_ehsize = sizeof(Elf128_Ehdr);
		elf.e_phentsize = sizeof(Elf128_Phdr);
		elf.e_phnum = sections.size();
		elf.e_shentsize = sizeof(Elf128_Shdr);
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
	if constexpr (VERBOSE_GLOBALS) {
	printf("------------------ Global symbols ------------------\n");
	for (const auto& symbol : assembler.globals())
	{
		auto addr = assembler.address_of(symbol);
		printf("\tGLOBAL\t  %s\t  0x%s\n",
			symbol.c_str(),
			to_hex_string(addr).c_str());
	}
	printf("------------------ Global symbols ------------------\n");
	}
	const std::string binfile = outfile + ".bin";
	auto& text = assembler.section(".text");
	file_writer(binfile, text.output);
}

#include <stdexcept>
#include <unistd.h>
std::string load_file(const std::string& filename, const char* rpath)
{
    size_t size = 0;
    FILE* f = fopen(filename.c_str(), "rb");
    if (f == NULL) {
		if (rpath != NULL) {
			std::string relpath = std::string(rpath) + "/" + filename;
			f = fopen(relpath.c_str(), "rb");
		}
		if (f == NULL)
		throw std::runtime_error("Could not open file: " + filename);
	}
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string result;
	result.resize(size);
    if (size != fread(result.data(), 1, size, f))
    {
        fclose(f);
        throw std::runtime_error("Error when reading from file: " + filename);
    }
    fclose(f);
    return result;
}

bool file_writer(const std::string& filename, const std::vector<uint8_t>& bin)
{
    FILE* f = fopen(filename.c_str(), "wb");
    if (f == NULL)
		return false;

	const size_t n = fwrite(bin.data(), 1, bin.size(), f);
    fclose(f);
	return n == bin.size();
}
