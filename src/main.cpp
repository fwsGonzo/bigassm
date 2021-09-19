#include "assembler.hpp"
#include "tokenizer.hpp"
#include "elf128.h"
#include <sstream>
extern std::string load_file(const std::string&);
static bool file_writer(const std::string&, const std::vector<uint8_t>&);
static std::vector<RawToken> split(const std::string&);
static constexpr bool VERBOSE_WORDS = false;
static constexpr bool VERBOSE_TOKENS = false;
static constexpr bool USE_ELF128 = true;

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "%s [asm ...] [bin]\n", argv[0]);
		exit(1);
	}
	const std::string outfile = argv[argc-1];

	std::vector<RawToken> raw_tokens;
	for (int i = 1; i < argc-1; i++)
	{
		const std::string infile = argv[1];
		auto input = load_file(infile);
		auto inwords = split(input);
		raw_tokens.insert(raw_tokens.end(), inwords.begin(), inwords.end());
	}

	if constexpr (VERBOSE_WORDS) {
	for (const auto& rt : raw_tokens)
		printf("Word: %s\n", rt.name.c_str());
	}
	auto tokens = Tokenizer::parse(raw_tokens);

	printf("Tokens: %zu  Lines: %u\n",
		tokens.size(), raw_tokens.back().line);

	if constexpr (VERBOSE_TOKENS) {
	for (auto& token : tokens)
		printf("Token: %s\n", token.to_string().c_str());
	}

	Options options;
	std::vector<uint8_t> output;
	Assembler assembler(options, tokens, output);
	assembler.assemble();

	if constexpr (USE_ELF128) {
		std::vector<uint8_t> elfbin;
		struct ElfData {
			Elf128_Ehdr hdr;
			Elf128_Phdr phdr[1];
		};
		elfbin.reserve(sizeof(ElfData) + output.size());
		elfbin.resize(sizeof(ElfData));
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
		elf.e_entry = assembler.address_of("_start");
		elf.e_phoff = offsetof(ElfData, phdr);
		elf.e_shoff = 0;
		elf.e_ehsize = sizeof(Elf128_Ehdr);
		elf.e_phentsize = sizeof(Elf128_Phdr);
		elf.e_phnum = 1;
		elf.e_shentsize = sizeof(Elf128_Shdr);
		elf.e_shnum = 0;
		elf.e_shstrndx = SHN_UNDEF;
		auto& program = elfdata->phdr[0];
		program.p_type = PT_LOAD;
		program.p_flags = PF_R | PF_W | PF_X;
		program.p_offset = sizeof(ElfData);
		program.p_vaddr = assembler.base_address();
		program.p_paddr = assembler.base_address();
		program.p_filesz = output.size();
		program.p_memsz = output.size();
		program.p_align = 0x10;
		elfbin.insert(elfbin.end(), output.begin(), output.end());
		file_writer(outfile, elfbin);
		printf("Written %zu bytes to %s\n", elfbin.size(), outfile.c_str());
	}
	const std::string binfile = outfile + ".bin";
	file_writer(binfile, output);
	printf("Written %zu bytes to %s\n", output.size(), binfile.c_str());
}

#define FLUSH_WORD() \
	if (!word.empty()) {	\
		tokens.push_back({word, line});	\
		word.clear();	\
	}

std::vector<RawToken> split(const std::string& s)
{
	std::vector<RawToken> tokens;

	std::string word;
	uint32_t line = 1;
	bool begin_quotes = false;
	bool begin_comment = false;
	for (char c : s)
	{
		if (begin_quotes) {
			word.append(1, c);
			if (c == '"') {
				tokens.push_back({word, line});
				word.clear();
				begin_quotes = false;
			}
			continue;
		}
		if (begin_comment) {
			if (c == '\n') {
				begin_comment = false;
			}
			continue;
		}
		switch (c) {
		case ';':
			begin_comment = true;
			continue;
		case '+':
		case '-':
		case '*':
			/* This allows building constant chains */
			FLUSH_WORD();
			word.append(1, c);
			break;
		case ',':
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			FLUSH_WORD();
			if (c == '\n') line++;
			break;
		case '"':
			begin_quotes = true;
			[[fallthrough]];
		default:
			word.append(1, c);
		}
	}
	if (!word.empty())
		tokens.push_back({word, line});

    return tokens;
}

#include <stdexcept>
#include <unistd.h>
std::string load_file(const std::string& filename)
{
    size_t size = 0;
    FILE* f = fopen(filename.c_str(), "rb");
    if (f == NULL) throw std::runtime_error("Could not open file: " + filename);

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
