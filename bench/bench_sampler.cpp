#include "maskaglia.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

static constexpr unsigned samples = 1000000;

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

static void bench_one(const char *name, sample_fn sampler_fn, unsigned parameter_set, unsigned coset)
{
	std::uint64_t state = UINT64_C(0x42454E434853414D) ^ (static_cast<std::uint64_t>(parameter_set) << 8) ^ coset ^ static_cast<std::uint8_t>(name[0]);
	std::int64_t total = 0;
	std::clock_t start, end;
	unsigned i;

	start = std::clock();
	for (i = 0; i < samples; i++)
	{
		std::int16_t sample;

		if (sampler_fn(&sample, parameter_set, coset, randomcoins, &state) != 0)
		{
			std::puts("sampler failed");
			return;
		}
		total += sample;
	}
	end = std::clock();

	std::printf("%s %u coset %u: %.2f ns/sample (%lld)\n", name, parameter_set, coset, static_cast<double>(end - start) * 1.0e9 / CLOCKS_PER_SEC / samples, static_cast<long long>(total));
}

int main()
{
	static constexpr std::array<unsigned, 3> parameter_sets = { 256, 512, 1024 };

	for (unsigned parameter_set : parameter_sets)
	{
		for (unsigned coset = 0; coset < 2; coset++)
		{
			bench_one("cdt", cdt_sample_ref, parameter_set, coset);
			bench_one("knuth-yao", knuth_yao_sample_ref, parameter_set, coset);
		}
	}

	return 0;
}
