/*
 * The SBR and PS decoders' fixed-point arithmetic (0x853078-0x853466,
 * with the 64-bit normalisation at 0x8c37d8): 32-bit values are Q28,
 * wide values are 64-bit at a precision of `fraction` bits.
 */
#include "aac.h"

#define Q28_MAX		0x07ffffff
#define Q28_MIN		(-0x08000000)

/* redundant sign bits */
static int32_t signBits64(int64_t value)
{
	int32_t high = (int32_t)(value >> 32), low = (int32_t)value;

	if ((high == 0 && low >= 0) || (high == -1 && low < 0))
		return aacSignBits32(low) + 32;
	return aacSignBits32(high);
}

/* the bits a value can be shifted left by, less 8 */
int32_t aacFixedNorm64(const int64_t *value)
{
	return signBits64(*value) - 8;
}

int32_t aacFixedSaturate(const int64_t *value)
{
	if (*value >= (int64_t)Q28_MAX + 1)
		return Q28_MAX;
	if (-*value >= (int64_t)Q28_MAX + 1)
		return Q28_MIN;
	return (int32_t)*value;
}

int32_t aacFixedToInt(int64_t *value, int32_t fraction)
{
	*value >>= fraction - 28;
	return aacFixedSaturate(value);
}

void aacFixedFromInt(int64_t *out, int32_t value, int32_t fraction)
{
	*out = (int64_t)value << (fraction - 28);
}

int32_t aacFixedShiftLeft(int32_t value, int32_t shift, int32_t fraction)
{
	int64_t wide, shifted;

	if (shift < 0)
		return aacFixedShiftRight(value, -shift, fraction);
	aacFixedFromInt(&wide, value, fraction);
	aacFixedShiftLeft64(&shifted, &wide, aacMin(shift, 31), fraction);
	return aacFixedToInt(&shifted, fraction);
}

int32_t aacFixedShiftRight(int32_t value, int32_t shift, int32_t fraction)
{
	int64_t wide, shifted;

	if (shift < 0)
		return aacFixedShiftLeft(value, -shift, fraction);
	aacFixedFromInt(&wide, value, fraction);
	aacFixedShiftRight64(&shifted, &wide, aacMin(shift, 31), fraction);
	return aacFixedToInt(&shifted, fraction);
}

/* clamps to fraction + 7 bits */
static void aacFixedSaturate64(int64_t *out, const int64_t *value, int32_t fraction)
{
	int64_t limit = (int64_t)1 << (fraction + 6);

	if (*value >= limit)
		*out = limit - 1;
	else if (-*value >= limit)
		*out = ~(limit - 1);
	else
		*out = *value;
}

int32_t aacFixedNorm(int32_t value)
{
	return (int8_t)(aacSignBits32(value) - 4);
}

/* two Q28 values' product at the given precision */
void aacFixedMultiply(int64_t *out, int32_t a, int32_t b, int32_t fraction)
{
	*out = ((int64_t)a * b) << (fraction - 55);
}

void aacFixedAdd(int64_t *out, int32_t a, int32_t b, int32_t fraction)
{
	int64_t wideA, wideB;

	aacFixedFromInt(&wideA, a, fraction);
	aacFixedFromInt(&wideB, b, fraction);
	*out = wideA + wideB;
}

void aacFixedShiftLeft64(int64_t *out, const int64_t *value, int32_t shift, int32_t fraction)
{
	int64_t shifted;
	int32_t headroom;

	if (shift < 0) {
		aacFixedShiftRight64(out, value, -shift, fraction);
		return;
	}
	shift = aacMin(shift, fraction);
	headroom = *value ? aacFixedNorm64(value) - fraction : 0;
	if (headroom + 63 < shift)
		shift = 63 - fraction + aacFixedNorm64(value);
	shifted = *value << shift;
	aacFixedSaturate64(out, &shifted, fraction);
}

void aacFixedShiftRight64(int64_t *out, const int64_t *value, int32_t shift, int32_t fraction)
{
	if (shift < 0) {
		aacFixedShiftLeft64(out, value, -shift, fraction);
		return;
	}
	*out = *value >> aacMin(shift, fraction);
}

void aacSbrFill(int8_t *to, int8_t value, int32_t count)
{
	while (count-- > 0)
		*to++ = value;
}

void aacSbrCopy(int8_t *to, const int8_t *from, int32_t count)
{
	while (count-- > 0)
		*to++ = *from++;
}

void aacSbrCopy12(int32_t *to, const int32_t *from)
{
	int32_t i;

	for (i = 0; i < 12; i++)
		to[i] = from[i];
}

/* k0, the first SBR band, by bs_start_freq (ISO/IEC 14496-3 4.6.18.3.2.1) */
static const uint8_t startBands16000[16] = {
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};
static const uint8_t startBands22050[16] = {
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 28, 30,
};
static const uint8_t startBands32000[16] = {
	10, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25, 27, 29, 32,
};
static const uint8_t startBands44100[16] = {
	8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 23, 25, 28, 32,
};
static const uint8_t startBands64000[16] = {
	6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21, 23, 26, 30,
};
static const uint8_t startBands88200[16] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 23, 27, 31,
};
static const uint8_t startBands24000[16] = {
	11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25, 27, 29, 32,
};
static const uint8_t startBands48000[16] = {
	7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 27, 31,
};

uint8_t aacSbrStartBand(uint32_t rate, uint32_t index)
{
	const uint8_t *table;

	switch (rate) {
	case 16000:
		table = startBands16000;
		break;
	case 22050:
		table = startBands22050;
		break;
	case 32000:
		table = startBands32000;
		break;
	case 44100:
		table = startBands44100;
		break;
	case 64000:
		table = startBands64000;
		break;
	case 88200:
	case 96000:
		table = startBands88200;
		break;
	case 24000:
		table = startBands24000;
		break;
	case 48000:
		table = startBands48000;
		break;
	default:
		return 0xff;
	}
	return table[index];
}
