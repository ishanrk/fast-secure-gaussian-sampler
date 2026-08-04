#include "tools.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

static void test_reverse()
{
	for (unsigned bit = 0; bit < 32; bit++)
	{
		std::uint32_t input = UINT32_C(1) << bit, expected = UINT32_C(1) << (31u - bit);

		assert(reverse_bits32(input) == expected);
	}

	std::uint32_t input = UINT32_C(0x01234567);
	assert(reverse_bits32(reverse_bits32(input)) == input);
}

static void test_bitslice()
{
	std::array<std::uint32_t, 32> input{}, planes{}, output{}, inplace{};

	for (std::size_t i = 0; i < input.size(); i++)
	{
		input[i] = UINT32_C(0x9E3779B9) * static_cast<std::uint32_t>(i + 1) ^ UINT32_C(0xA5A5A5A5);
	}

	bitslice32(planes.data(), input.data());
	for (unsigned bit = 0; bit < 32; bit++)
	{
		for (unsigned lane = 0; lane < 32; lane++)
		{
			std::uint32_t found = (planes[bit] >> lane) & 1u, expected = (input[lane] >> bit) & 1u;

			assert(found == expected);
		}
	}

	unbitslice32(output.data(), planes.data());
	assert(output == input);

	inplace = input;
	bitslice32(inplace.data(), inplace.data());
	assert(inplace == planes);
	unbitslice32(inplace.data(), inplace.data());
	assert(inplace == input);
}

int main()
{
	test_reverse();
	test_bitslice();

	return 0;
}
