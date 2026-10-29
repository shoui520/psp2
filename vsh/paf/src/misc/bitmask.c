/* Counter bitmasks and quantized dimensions. */
#include <stdint.h>

typedef struct PafBitmaskState {
	uint32_t reserved[3];
	uint32_t allocated;
	uint32_t unused;
	uint32_t count;
} PafBitmaskState;

typedef struct PafQuantizedDimensions {
	uint8_t reserved[28];
	int32_t x;
	int32_t y;
	uint8_t reserved2[36];
	int32_t scalingMode;
	uint8_t reserved3[6];
	uint16_t quantum;
} PafQuantizedDimensions;

typedef struct PafQuantizedOwner {
	uint8_t reserved[44];
	PafQuantizedDimensions *dimensions;
} PafQuantizedOwner;

/* The original rounds signed values toward positive infinity on each halving. */
static int32_t HalfRoundUp(int32_t value)
{
	uint32_t next = (uint32_t)value + 1;

	next += next >> 31;
	return (int32_t)next >> 1;
}

uint32_t ScePafMisc_1805D207(PafBitmaskState *state, uint32_t bit)
{
	uint32_t mask = (bit & 255) < 32 ? 1U << (bit & 31) : 0;

	if (state->count && (state->allocated & mask))
		return state->count;
	state->count++;
	state->allocated |= mask;
	return state->count;
}

void ScePafMisc_E80F175C(int32_t *output,
			 const PafQuantizedDimensions *dimensions, uint32_t halves)
{
	int32_t x = dimensions->x;
	int32_t y = dimensions->y;

	if (dimensions->scalingMode == 1)
		while (halves--) {
			x = HalfRoundUp(x);
			y = HalfRoundUp(y);
		}
	output[0] = x;
	output[1] = y;
}

int32_t ScePafMisc_EA6558AD(const void *unused, uint32_t bits,
			    uint32_t ordinal)
{
	uint32_t bit = 0, seen = 0;

	(void)unused;
	while (bits) {
		if (bits & 1U) {
			if (seen == ordinal)
				return (int32_t)bit;
			seen++;
		}
		bits >>= 1;
		bit++;
	}
	return -1;
}

int32_t ScePafMisc_49034205(PafBitmaskState *state, uint32_t bit)
{
	uint32_t mask = (bit & 255) < 32 ? 1U << (bit & 31) : 0;

	if (!state->count || !(state->allocated & mask))
		return -1;
	state->allocated &= ~mask;
	return --state->count;
}

int32_t ScePafMisc_2B325CC6(const PafQuantizedDimensions *dimensions,
			     uint32_t halves)
{
	int32_t dimension = dimensions->y;
	int32_t quantum = dimensions->quantum;
	int32_t numerator;
	int32_t quotient;

	if (dimensions->scalingMode == 1)
		while (halves--)
			dimension = HalfRoundUp(dimension);
	numerator = (int32_t)((uint32_t)dimension + quantum - 1);
	quotient = numerator / quantum;
	return (int32_t)((uint32_t)quotient * (uint32_t)quantum);
}

int32_t ScePafMisc_9CE18975(const PafQuantizedOwner *owner, uint32_t halves)
{
	return ScePafMisc_2B325CC6(owner->dimensions, halves);
}
