/*
 * The scaling lists of the parameter sets, as PU0 parsed them: both codecs' (PU1's,
 * and FW2's codec). Each list's deltas come four to a word of g_parsed.
 */
#include "h264.h"

/* the scans that put the scaling lists into raster order */
static const uint8_t g_scan4x4[16] = {
	0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

static const uint8_t g_scan8x8[64] = {
	0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

/* the default scaling lists (Default_4x4_Intra, ..._Inter, Default_8x8_...) */
const uint8_t g_default4x4[2][16] = {
	{ 6, 13, 20, 28, 13, 20, 28, 32, 20, 28, 32, 37, 28, 32, 37, 42 },
	{ 10, 14, 20, 24, 14, 20, 24, 27, 20, 24, 27, 30, 24, 27, 30, 34 },
};

const uint8_t g_default8x8[2][64] = {
	{
		6, 10, 13, 16, 18, 23, 25, 27, 10, 11, 16, 18, 23, 25, 27, 29,
		13, 16, 18, 23, 25, 27, 29, 31, 16, 18, 23, 25, 27, 29, 31, 33,
		18, 23, 25, 27, 29, 31, 33, 36, 23, 25, 27, 29, 31, 33, 36, 38,
		25, 27, 29, 31, 33, 36, 38, 40, 27, 29, 31, 33, 36, 38, 40, 42,
	}, {
		9, 13, 15, 17, 19, 21, 22, 24, 13, 13, 17, 19, 21, 22, 24, 25,
		15, 17, 19, 21, 22, 24, 25, 27, 17, 19, 21, 22, 24, 25, 27, 28,
		19, 21, 22, 24, 25, 27, 28, 30, 21, 22, 24, 25, 27, 28, 30, 32,
		22, 24, 25, 27, 28, 30, 32, 33, 24, 25, 27, 28, 30, 32, 33, 35,
	},
};

/*
 * scaling_list(): the deltas come four to a word from g_parsed[*word]; a list whose
 * first delta makes the next scale 0 takes the default list.
 */
static void takeScalingList(uint8_t *list, const uint8_t *scan, uint32_t size,
	const uint8_t *defaults, uint32_t *word)
{
	uint32_t last = 8, next = 8, value, useDefault = 0, i;

	for (i = 0; i < size; i++) {
		if (next)
			next = (last + (g_parsed[*word] >> (24 - 8 * (i & 3)) & 0xff) + 256) & 0xff;
		if (!next && !i)
			useDefault = 1;
		value = next ? next : last;
		list[scan[i]] = value;
		last = value;
		if ((i & 3) == 3)
			(*word)++;
	}
	if (useDefault) {
		for (i = 0; i < size; i++)
			list[i] = defaults[i];
	}
}

/*
 * The scaling lists of a parameter set: a word of present flags, then four words
 * for each 4x4 list and sixteen for each 8x8 one, there or not.
 */
uint32_t takeScalingMatrix(VipScalingMatrix *matrix, uint8_t *present, uint32_t word)
{
	uint32_t flags = g_parsed[word++], i;

	for (i = 0; i < VIP_SCALING_LISTS; i++)
		present[i] = flags >> (31 - i) & 1;
	for (i = 0; i < 6; i++) {
		if (flags >> (31 - i) & 1)
			takeScalingList(matrix->lists4x4[i], g_scan4x4, 16,
				g_default4x4[i >= 3], &word);
		else
			word += 4;
	}
	for (i = 6; i < VIP_SCALING_LISTS; i++) {
		if (flags >> (31 - i) & 1)
			takeScalingList(matrix->lists8x8[i - 6], g_scan8x8, 64,
				g_default8x8[i != 6], &word);
		else
			word += 16;
	}
	return word;
}
