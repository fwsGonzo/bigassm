#include "types.hpp"

static size_t to_hex(char* buffer, size_t len, __uint128_t value)
{
	if (len < 32) return 0;
	len = 8; /* At least print 8 hex digits */
	static constexpr char lut[] = "0123456789ABCDEF";
	for (unsigned i = 0; i < 16 - len / 2; i++) {
		if ((value >> ((15-i) * 8)) & 0xFF) {
			len = 32 - i * 2;
			break;
		}
	}
	const size_t max = len / 2;
	for (unsigned i = 0; i < max; i++) {
		buffer[i*2 + 0] = lut[(value >> ((max-1-i) * 8 + 4)) & 0xF];
		buffer[i*2 + 1] = lut[(value >> ((max-1-i) * 8 + 0)) & 0xF];
	}
	return len;
}

std::string to_hex_string(__uint128_t value)
{
	std::string result;
	result.resize(32);
	to_hex(result.data(), result.size(), value);
	return result;
}
