#include "assembler.hpp"
#include "tokenizer.hpp"
#include <sstream>
extern std::string load_file(const std::string&);
static bool file_writer(const std::string&, const std::vector<uint8_t>&);
static std::vector<RawToken> split(const std::string&);
static constexpr bool VERBOSE_WORDS = false;
static constexpr bool VERBOSE_TOKENS = false;

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

	file_writer(outfile, output);
	printf("Written %zu bytes to %s\n", output.size(), outfile.c_str());
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
