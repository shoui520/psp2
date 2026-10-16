/*
 * Joint stereo (ISO/IEC 11172-3 2.4.3.4.9, 13818-3 2.4.3.2; VLIW,
 * 0x8e05f8-0x8e08f0), two samples at a time.
 */
#include "mp3.h"

#define SQRT_HALF	0x5a827999	/* 1/sqrt(2), Q31 */

/* is_ratio / (1 + is_ratio) for is_pos 0-6, is_ratio = tan(is_pos pi / 12) */
static const int32_t intensityRatios[7] = {
	0, 453816692, 786033569, 1073741824, 1361450078, 1693666955, 2147483645,
};

/* 0x8e05f8: L = (M + S) / sqrt(2), R = (M - S) / sqrt(2) */
void mp3StereoMs(int32_t *left, int32_t *right, uint32_t count)
{
	uint32_t i;

	for (i = 0; i < (count & ~1u); i++) {
		int32_t mid = left[i], side = right[i];

		left[i] = ivc2Multiply(ivc2AddSaturate(mid, side), SQRT_HALF);
		right[i] = ivc2Multiply(ivc2SubtractSaturate(mid, side), SQRT_HALF);
	}
}

/* 0x8e0690: MPEG-1 intensity, the left channel split by is_pos */
void mp3StereoIntensity(int32_t *left, int32_t *right, int32_t position, uint32_t count)
{
	int32_t toLeft = intensityRatios[position], toRight = intensityRatios[6 - position];
	uint32_t i;

	for (i = 0; i < (count & ~1u); i++) {
		int32_t value = left[i];

		left[i] = ivc2Multiply(value, toLeft);
		right[i] = ivc2Multiply(value, toRight);
	}
}

/*
 * 0x8e0750: MPEG-2 intensity: position 0 copies the left channel; otherwise
 * the ratio k of (position + 1) / 2 scales the left channel for odd
 * positions (the right keeps it unscaled) and the right for even ones.
 */
void mp3StereoIntensityLsf(int32_t *left, int32_t *right, int32_t position,
	const int32_t *ratios, uint32_t count)
{
	int32_t ratio;
	uint32_t i;

	if (position == 0) {
		for (i = 0; i < (count & ~1u); i++)
			right[i] = left[i];
		return;
	}
	ratio = ratios[(position - 1) >> 1];
	for (i = 0; i < (count & ~1u); i++) {
		int32_t value = left[i];

		if (position & 1) {
			right[i] = value;
			left[i] = ivc2Multiply(value, ratio);
		} else {
			right[i] = ivc2Multiply(value, ratio);
		}
	}
}
