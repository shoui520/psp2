/*
 * Perceptual noise substitution's noise (0x851414-0x85173c): a band of
 * random values scaled to unit energy, then to the band's gain. Values
 * are Q23; the core uses the coprocessor's 64-bit accumulators.
 */
#include "aac.h"

#define NOISE_MULTIPLIER	0x19660d
#define NOISE_INCREMENT		0x3c6ef35f
#define SQRT_HALF		0x5a8279	/* 1/sqrt(2), Q23 */
#define Q23			0x800000

/* cpsla3.w: shifts left by the count's low five bits, saturating */
static int32_t mulQ23(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b) >> 23);
}

/* the shift that puts the energy's leading sign bit at bit 45 */
static int32_t aacNoiseNormalize(int64_t *energy)
{
	int64_t value = *energy, top = -((int64_t)1 << 46);
	int32_t shift = 0;

	if (value == 0)
		return 46;
	if (value > 0)
		value = ~value;
	while ((value & top) != top) {
		value >>= 1;
		shift--;
	}
	while (value & ((int64_t)1 << 45)) {
		value <<= 1;
		shift++;
	}
	*energy = value;
	return shift;
}

static void aacNoiseRandom(AacNoise *noise, int32_t *out, int32_t count)
{
	uint32_t seed = noise->seed;
	int32_t i;

	for (i = 0; i < count; i++) {
		seed = seed * NOISE_MULTIPLIER + NOISE_INCREMENT;
		out[i] = (int32_t)seed >> 8;
	}
	noise->seed = seed;
}

void aacNoiseGenerate(AacNoise *noise, int32_t *out, int32_t count, int32_t scale,
	int32_t exponent, int32_t *buffer)
{
	int64_t energy = 0, normal;
	int32_t i, shift, level, index, fraction, gain, power;

	aacNoiseRandom(noise, buffer, count);
	for (i = 0; i < (count >> 1) * 2; i++)
		energy += (int64_t)buffer[i] * buffer[i];

	normal = energy;
	shift = aacNoiseNormalize(&normal);
	if (shift >= 1)
		energy <<= shift;
	else
		energy >>= -shift;
	level = (int32_t)(energy >> 23);

	/* 1/sqrt(energy), interpolated between the table's neighbours */
	index = (level >> 14) & 255;
	fraction = (level << 9) & (Q23 - 1);
	gain = (int32_t)(((int64_t)noise->inverseSqrt[index + 1] * fraction -
		(int64_t)noise->inverseSqrt[index] * (fraction - Q23)) >> 23);
	if (-shift & 1)
		gain = mulQ23(gain, SQRT_HALF);
	shift = 1 - (-shift >> 1);
	if (gain < Q23 / 2) {
		shift--;
		gain <<= 1;
	}

	exponent += shift;
	power = noise->negativePowers[0];
	if (exponent < 0 && exponent >= -23) {
		power = noise->negativePowers[-exponent];
		exponent = 0;
	}
	gain = mulQ23(mulQ23(gain, scale), -power);
	for (i = 0; i < count; i++)
		out[i] = aacShiftSaturate(mulQ23(buffer[i], gain), exponent);
}
