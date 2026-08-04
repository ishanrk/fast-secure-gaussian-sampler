#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <cstdint>

/* Reverse all bits in a 32-bit word. */
std::uint32_t reverse_bits32(std::uint32_t value) noexcept;

/* Transpose 32 words into 32 bit planes. Input may equal output. */
void bitslice32(std::uint32_t output[32], const std::uint32_t input[32]) noexcept;

/* Restore 32 words from 32 bit planes. Input may equal output. */
void unbitslice32(std::uint32_t output[32], const std::uint32_t input[32]) noexcept;

#endif
