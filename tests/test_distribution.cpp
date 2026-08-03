#include "maskaglia.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

static constexpr unsigned samples = 1000000;
static constexpr int limit = 32;

using sample_fn = int (*)(std::int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

static std::uint64_t randomword(std::uint64_t *state)
{
	std::uint64_t value;

	*state += UINT64_C(0x9E3779B97F4A7C15);
	value = *state;
	value = (value ^ (value >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	value = (value ^ (value >> 27)) * UINT64_C(0x94D049BB133111EB);

	return value ^ (value >> 31);
}

static int randomcoins(void *context, std::uint8_t *output, std::size_t output_length)
{
	auto *state = static_cast<std::uint64_t *>(context);
	std::size_t i, take;

	while (output_length > 0)
	{
		std::uint64_t word = randomword(state);

		take = output_length < 8 ? output_length : 8;
		for (i = 0; i < take; i++)
		{
			output[i] = static_cast<std::uint8_t>(word >> (8u * i));
		}
		output += take;
		output_length -= take;
	}

	return 0;
}

static int badcoins(void *context, std::uint8_t *output, std::size_t output_length)
{
	static_cast<void>(context);
	static_cast<void>(output);
	static_cast<void>(output_length);

	return -1;
}

static double sigma(unsigned parameter_set)
{
	switch (parameter_set)
	{
		case 256:
			return 1.010;
		case 512:
			return 1.278;
		case 1024:
			return 1.299;
		default:
			return 0.0;
	}
}

static int same_coset(int sample, unsigned coset)
{
	unsigned value = static_cast<unsigned>(sample < 0 ? -sample : sample);

	return (value & 1u) == coset;
}

static double weight(int sample, double width)
{
	double value = static_cast<double>(sample);

	return std::exp(-(value * value) / (8.0 * width * width));
}

static void test_errors(void)
{
	std::uint64_t state = UINT64_C(1);
	std::int16_t sample = 7;

	assert(maskaglia_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(nullptr, 256, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 2, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 0, nullptr, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 0, badcoins, &state) != 0);
	assert(knuth_yao_sample_ref(nullptr, 256, 0, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 2, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 0, nullptr, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 0, badcoins, &state) != 0);
	assert(sample == 7);
}

static void test_sampler(const char *name, sample_fn sampler_fn, unsigned parameter_set, unsigned coset)
{
	std::uint64_t state = UINT64_C(0x53414D504C455246) ^ (static_cast<std::uint64_t>(parameter_set) << 8) ^ coset ^ static_cast<std::uint8_t>(name[0]);
	std::array<std::uint64_t, 2 * limit + 1> frequency{};
	double total = 0.0, squares = 0.0, norm = 0.0, expected_variance = 0.0, max_diff = 0.0, mean, variance;
	double width = sigma(parameter_set);
	unsigned i;
	int result, sample, x;

	for (x = -limit; x <= limit; x++)
	{
		if (same_coset(x, coset))
		{
			double value = weight(x, width);

			norm += value;
			expected_variance += static_cast<double>(x * x) * value;
		}
	}
	expected_variance /= norm;

	for (i = 0; i < samples; i++)
	{
		std::int16_t value;

		result = sampler_fn(&value, parameter_set, coset, randomcoins, &state);
		assert(result == 0);
		assert(same_coset(value, coset));
		assert(value >= -limit && value <= limit);
		sample = value;
		frequency[static_cast<std::size_t>(sample + limit)]++;
		total += sample;
		squares += static_cast<double>(sample) * sample;
	}

	mean = total / samples;
	variance = squares / samples - mean * mean;

	for (x = -limit; x <= limit; x++)
	{
		double expected = same_coset(x, coset) ? weight(x, width) / norm : 0.0;
		double found = static_cast<double>(frequency[static_cast<std::size_t>(x + limit)]) / samples;
		double diff = std::fabs(found - expected);

		if (diff > max_diff)
		{
			max_diff = diff;
		}
	}

	std::printf("%s %u coset %u: mean %.4f variance %.4f max diff %.5f\n", name, parameter_set, coset, mean, variance, max_diff);
	assert(std::fabs(mean) < 0.01);
	assert(std::fabs(variance - expected_variance) < 0.03);
	assert(max_diff < 0.0025);
}

int main()
{
	static constexpr std::array<unsigned, 3> parameter_sets = { 256, 512, 1024 };

	test_errors();
	for (unsigned parameter_set : parameter_sets)
	{
		for (unsigned coset = 0; coset < 2; coset++)
		{
			test_sampler("cdt", cdt_sample_ref, parameter_set, coset);
			test_sampler("knuth-yao", knuth_yao_sample_ref, parameter_set, coset);
		}
	}

	return 0;
}
