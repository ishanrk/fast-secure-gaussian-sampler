#include "maskaglia.hpp"

int maskaglia_sample_ref(std::int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context)
{
	static_cast<void>(sample);
	static_cast<void>(parameter_set);
	static_cast<void>(coset);
	static_cast<void>(randombytes);
	static_cast<void>(random_context);

	return -1;
}
