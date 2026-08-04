#include "tools.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

std::uint32_t reverse_bits32(std::uint32_t value) noexcept
{
	value = ((value >> 1) & UINT32_C(0x55555555)) | ((value & UINT32_C(0x55555555)) << 1);
	value = ((value >> 2) & UINT32_C(0x33333333)) | ((value & UINT32_C(0x33333333)) << 2);
	value = ((value >> 4) & UINT32_C(0x0F0F0F0F)) | ((value & UINT32_C(0x0F0F0F0F)) << 4);
	value = ((value >> 8) & UINT32_C(0x00FF00FF)) | ((value & UINT32_C(0x00FF00FF)) << 8);

	return (value >> 16) | (value << 16);
}

static void transpose32(std::array<std::uint32_t, 32> &words) noexcept
{
	static constexpr std::array<unsigned, 5> shifts = { 16, 8, 4, 2, 1 };
	static constexpr std::array<std::uint32_t, 5> masks = {
		UINT32_C(0xFFFF0000), UINT32_C(0xFF00FF00), UINT32_C(0xF0F0F0F0),
		UINT32_C(0xCCCCCCCC), UINT32_C(0xAAAAAAAA)
	};

	for (std::size_t stage = 0; stage < shifts.size(); stage++)
	{
		unsigned shift = shifts[stage];
		std::uint32_t mask = masks[stage];

		for (unsigned base = 0; base < 32; base += 2 * shift)
		{
			for (unsigned i = 0; i < shift; i++)
			{
				unsigned left = base + i, right = left + shift;
				std::uint32_t swap = (words[left] ^ (words[right] << shift)) & mask;

				words[left] ^= swap;
				words[right] ^= swap >> shift;
			}
		}
	}
}

void bitslice32(std::uint32_t output[32], const std::uint32_t input[32]) noexcept
{
	std::array<std::uint32_t, 32> words{};

	for (std::size_t i = 0; i < words.size(); i++)
	{
		words[i] = input[i];
	}
	transpose32(words);
	for (std::size_t i = 0; i < words.size(); i++)
	{
		output[i] = words[i];
	}
}

void unbitslice32(std::uint32_t output[32], const std::uint32_t input[32]) noexcept
{
	bitslice32(output, input);
}
