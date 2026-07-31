#ifndef MASKAGLIA_H
#define MASKAGLIA_H

#include <stddef.h>
#include <stdint.h>

typedef int (*maskaglia_randombytes_fn)(void *context, uint8_t *output, size_t output_length);

int maskaglia_sample_ref(int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);
int cdt_sample_ref(int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

#endif
