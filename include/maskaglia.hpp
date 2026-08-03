#ifndef MASKAGLIA_HPP
#define MASKAGLIA_HPP

#include <cstddef>
#include <cstdint>

using maskaglia_randombytes_fn = int (*)(void *context, std::uint8_t *output, std::size_t output_length);

/* Scalar Maskaglia reference. Not implemented. */
int maskaglia_sample_ref(std::int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

/* Scalar cumulative distribution table reference. */
int cdt_sample_ref(std::int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

/* Scalar Knuth-Yao discrete distribution tree reference. */
int knuth_yao_sample_ref(std::int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

#endif
