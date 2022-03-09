#include "assembler.hpp"
#include "elf128.h"
#include <cstring>
#include <libgen.h>
extern std::string load_file(const std::string&, const char* = nullptr);
extern bool file_writer(const std::string&, const std::vector<uint8_t>&);
extern std::vector<RawToken> split(const std::string&);
static constexpr bool VERBOSE_WORDS = false;
static constexpr bool VERBOSE_TOKENS = false;
static constexpr bool VERBOSE_GLOBALS = true;

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
		auto tokens = Assembler::split(input);

		if constexpr (VERBOSE_TOKENS) {
			for (auto& token : tokens)
				printf("Token: %s\n", token.to_string().c_str());
		}

		const char* rpath = get_realpath(infile.c_str());
		assembler.assemble(tokens, rpath);
	}

	assembler.finish();

	ELFwriter64(options, assembler, outfile + "64");
	ELFwriter128(options, assembler, outfile + "128");

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
