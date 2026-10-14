/*
 * The granule's reordering of short blocks (0x8e0348) and its PCM
 * (0x8e0138); VLIW kernels, two samples at a time.
 */
#include "mp3.h"

/*
 * 0x8e0348: the long subbands the antialiasing covers are copied as they
 * are (all 32 for long blocks); from scale factor band 3 of a mixed block,
 * or 0, the three short windows of each band are interleaved.
 */
void mp3Reorder(int32_t *xr, int32_t *work, const uint16_t *shortBounds,
	uint32_t antialiasBands)
{
	uint32_t sfb = 0, i, j, k;

	if (antialiasBands) {
		for (i = 0; i < antialiasBands * 18; i++)
			work[i] = xr[i];
		if (antialiasBands == 32)
			return;
		sfb = 3;
	}
	for (; sfb < MP3_SHORT_BANDS; sfb++) {
		uint32_t start = 3 * shortBounds[sfb];
		uint32_t width = shortBounds[sfb + 1] - shortBounds[sfb];

		for (j = 0; j < (width & ~1u); j++)
			for (k = 0; k < 3; k++)
				work[start + 3 * j + k] = xr[start + k * width + j];
	}
}

/*
 * 0x8e0138: each sample rounded to 16 bits (>> 11) or 24 bits (>> 3),
 * clipped, and written `stride` bytes apart in either byte order.
 */
void mp3Output(const int32_t *xr, uint8_t *output, uint32_t bytesPerSample,
	uint32_t count, uint32_t stride, uint32_t bigEndian)
{
	uint32_t i, k;

	for (i = 0; i < (count & ~1u); i++, output += stride) {
		int32_t value;

		if (bytesPerSample == 3) {
			value = ivc2Clip((xr[i] + 2) >> 3, 24);
			for (k = 0; k < 3; k++)
				output[k] = value >> (bigEndian == 1 ? 16 - 8 * k : 8 * k);
		} else {
			value = ivc2Clip((xr[i] + 1024) >> 11, 16);
			for (k = 0; k < 2; k++)
				output[k] = value >> (bigEndian == 1 ? 8 - 8 * k : 8 * k);
		}
	}
}
