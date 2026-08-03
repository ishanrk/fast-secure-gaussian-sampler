#include <cstddef>
#include <cstdint>

int randomcoins_shake(void *context, std::uint8_t *output, std::size_t output_length)
{
	static_cast<void>(context);
	static_cast<void>(output);
	static_cast<void>(output_length);

	return -1;
}
