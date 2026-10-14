/*
 * The ATRAC9 inverse MDCT's engine: runs the passes of at9imdct256.c,
 * at9imdct128.c and at9imdct64.c (see at9imdct.h).
 *
 * The coprocessor works on pairs of 32-bit words; every operation below is
 * the per-word meaning of its instruction (CPSUB3.W, CPMULSLLA1.W with the
 * high word taken and shifted left once, CPAVE3.W, CPSSUB3.W with CPADDSR3.W,
 * CPSMADSLLA1.W, CPMADLA1.W, CPMSBLA1.W, CPPACKLA0.H).
 */
#include "at9imdct.h"

#define MIN_INT32			(-2147483647 - 1)
#define MAX_INT32			2147483647
#define MAX_INT64			0x7fffffffffffffffLL
#define MIN_INT64			(-MAX_INT64 - 1)
#define MIN_INT16			(-32768)
#define MAX_INT16			32767

/* the accumulator's seed: CPMULSLLA1.W of 2^30 and 16 */
#define AT9_ROUNDING			((int64_t)1 << 35)

/* the high word of 2 * a * b, shifted left once (a Q31 product) */
static int32_t mulQ31(int32_t a, int32_t b)
{
	int64_t product;

	if (a == MIN_INT32 && b == MIN_INT32)
		product = MAX_INT64;
	else
		product = 2 * (int64_t)a * b;
	return (int32_t)((uint32_t)(product >> 32) << 1);
}

/* (a + b + 1) >> 1 without overflow */
static int32_t ave(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a + b + 1) >> 1);
}

/* the saturated difference, halved and rounded up */
static int32_t halfDifference(int32_t a, int32_t b)
{
	int64_t difference = (int64_t)a - b;

	if (difference > MAX_INT32)
		difference = MAX_INT32;
	else if (difference < MIN_INT32)
		difference = MIN_INT32;
	return (int32_t)((difference + 1) >> 1);
}

static int16_t saturate16(int32_t value)
{
	if (value > MAX_INT16)
		return MAX_INT16;
	if (value < MIN_INT16)
		return MIN_INT16;
	return (int16_t)value;
}

/* the sum of two 64-bit values, saturated (CPSMADSLLA1.W) */
static int64_t saturatedAdd(int64_t a, int64_t b)
{
	if (b > 0 && a > MAX_INT64 - b)
		return MAX_INT64;
	if (b < 0 && a < MIN_INT64 - b)
		return MIN_INT64;
	return a + b;
}

/*
 * The pair of 16-bit samples of the accumulator 2^35 + (a << 31) +- b * window.
 * With the minus sign the overlap is added first (CPSMADSLLA1.W), then the
 * product subtracted (CPMSBLA1.W); with the plus sign the product is added
 * first (CPMADLA1.W), then the overlap, saturated.
 */
static uint32_t windowSample(int32_t a, int32_t b, int32_t window, int sign, unsigned int shift)
{
	int64_t overlap = (int64_t)a << 31;
	int64_t product = (int64_t)b * window;
	int64_t accumulator;
	int32_t low, high;

	if (sign > 0) {
		accumulator = AT9_ROUNDING + product;
		accumulator = saturatedAdd(accumulator, overlap);
	} else {
		accumulator = saturatedAdd(AT9_ROUNDING, overlap);
		accumulator = (int64_t)((uint64_t)accumulator - (uint64_t)product);
	}
	low = (int32_t)accumulator >> shift;
	high = (int32_t)(accumulator >> 32) >> shift;
	return (uint16_t)saturate16(low) | (uint32_t)(uint16_t)saturate16(high) << 16;
}

static int32_t *wordAt(uint8_t *const *base, uint8_t table, uint16_t offset)
{
	return (int32_t *)(base[table] + offset);
}

static void runStep(int32_t *reg, const At9Step *step)
{
	int32_t a, m;

	switch (step->op) {
	case AT9_STEP_SUB:
		reg[step->d] = (int32_t)((uint32_t)reg[step->a] - (uint32_t)reg[step->b]);
		break;
	case AT9_STEP_MULQ:
		reg[step->d] = mulQ31(reg[step->a], reg[step->b]);
		break;
	case AT9_STEP_BFLY:
	case AT9_STEP_BFLY_REG:
		a = reg[step->a];
		m = mulQ31(reg[step->b], step->op == AT9_STEP_BFLY ? step->k : reg[step->c]);
		reg[step->d] = ave(a, m);
		reg[step->e] = halfDifference(a, m);
		break;
	}
}

static void runPass(const At9Pass *pass, uint8_t **base)
{
	int32_t reg[AT9_REGISTERS];
	unsigned int iteration, i, b;

	for (iteration = 0; iteration < pass->iterations; iteration++) {
		for (i = 0; i < pass->numInputs; i++)
			reg[i] = *wordAt(base, pass->inputs[i].base, pass->inputs[i].offset);
		for (i = 0; i < pass->numSteps; i++)
			runStep(reg, &pass->steps[i]);
		for (i = 0; i < pass->numOutputs; i++)
			*wordAt(base, pass->outputs[i].base, pass->outputs[i].offset) = reg[pass->outputs[i].reg];
		for (i = 0; i < pass->numSamples; i++) {
			const At9Sample *sample = &pass->samples[i];

			*(uint32_t *)wordAt(base, AT9_X, sample->offset) = windowSample(reg[sample->a],
				reg[sample->b], reg[sample->window], sample->sign, pass->shift);
		}
		for (b = 0; b < AT9_BASES; b++)
			base[b] += pass->advance[b];
	}
}

/* the transform of one channel: the spectrum in place, the overlap in the work area */
void at9ImdctRun(const At9Imdct *imdct, At9Channel *channel, At9Work *work)
{
	uint8_t *base[AT9_BASES] = { 0 };
	uint8_t *spectrum = (uint8_t *)channel->spectrum;
	const int32_t *tables = imdct->tables;

	base[AT9_L] = spectrum;
	base[AT9_R] = spectrum + imdct->rightColumn;
	runPass(imdct->first, base);

	base[AT9_X] = spectrum;
	base[AT9_OV] = (uint8_t *)work->overlap;
	base[AT9_WC] = (uint8_t *)tables;
	base[AT9_WD] = (uint8_t *)(tables + imdct->tableWords);
	base[AT9_TW] = (uint8_t *)(tables + 3 * imdct->tableWords - imdct->twiddleWords);
	base[AT9_WA] = (uint8_t *)(tables + 3 * imdct->tableWords);
	base[AT9_WB] = (uint8_t *)(tables + 4 * imdct->tableWords);
	runPass(imdct->second, base);
}
