/*
 * The filter bank's windowing and overlap-add (ISO/IEC 14496-3 4.6.11,
 * 0x8b4068-0x8b4e30). The IMDCT kernel leaves each window's DCT-IV of N
 * values in place; its 2N time samples are unfolded from them here: the
 * first half from the values at 3N/4 (even samples) and below N/4 (odd,
 * negated), the second half from the N/2 values at N/4, which a frame
 * keeps as its overlap. Products are Q31 with saturating sums.
 *
 * A long frame's overlap is the 512 values from spectrum[256]. After
 * eight short windows it holds time samples 1024-1055 of the frame at
 * [0], the DCT-IV values 32-95 of window 4 at [32], samples 1056-1087 at
 * [96], and windows 5-7 at [128].
 */
#include "aac.h"

#define SHORT_START	448	/* where the short windows (and the transitions) start */

static int32_t negate(int32_t value)
{
	return aacSubtractSaturate(0, value);
}

/* sample m < N of a window's first half; the second quarter mirrors the first, negated */
static int32_t rising(const int32_t *spectrum, int32_t m, int32_t length)
{
	int32_t n = m < length / 2 ? m : length - 1 - m;
	int32_t value = n & 1 ? spectrum[length / 4 - 1 - (n >> 1)] :
		spectrum[3 * length / 4 + (n >> 1)];

	return (n & 1) == (m < length / 2) ? negate(value) : value;
}

/* sample n < N/2 of the second half, from its N/2 values */
static int32_t secondSample(const int32_t *overlap, int32_t n, int32_t length)
{
	if (n & 1)
		return negate(overlap[length / 2 - 1 - (n >> 1)]);
	return overlap[n >> 1];
}

/* sample m < N of the second half */
static int32_t falling(const int32_t *overlap, int32_t m, int32_t length)
{
	if (m < length / 2)
		return secondSample(overlap, m, length);
	return secondSample(overlap, length - 1 - m, length);
}

static int32_t windowed(int32_t sample, int32_t window)
{
	return (int32_t)(aacProduct(sample, window) >> 32);
}

/* one short window's second half added to the next one's first */
static int32_t overlapped(const int32_t *overlap, const int32_t *next, int32_t r,
	const int32_t *window)
{
	return (int32_t)(aacAddWide(aacProduct(falling(overlap, r, 128), window[127 - r]),
		aacProduct(rising(next, r, 128), window[r])) >> 32);
}

/* 0x8b4738: after eight short windows */
void aacOverlapShort(int32_t *overlap, int32_t *work, const int32_t *window)
{
	int32_t k, r, m;

	for (m = 0; m < 32; m++) {
		work[m] = overlap[m];
		work[32 + m] = overlap[96 + m];
	}
	for (k = 0; k < 3; k++)
		for (r = 0; r < 128; r++)
			work[64 + 128 * k + r] = overlapped(overlap + 128 * k + 32,
				overlap + 128 * (k + 1), r, window);
	for (r = 0; r < 128; r++)
		work[SHORT_START + r] = windowed(falling(overlap + 3 * 128 + 32, r, 128),
			window[127 - r]);
	for (m = SHORT_START + 128; m < AAC_FRAME_SAMPLES; m++)
		work[m] = 0;
}

/* 0x8b4cf8: after a long start window: flat, the short window's fall, then zero */
void aacOverlapStart(int32_t *overlap, int32_t *work, const int32_t *window)
{
	int32_t m;

	for (m = 0; m < SHORT_START; m++)
		work[m] = falling(overlap, m, AAC_FRAME_SAMPLES);
	for (; m < SHORT_START + 128; m++)
		work[m] = windowed(falling(overlap, m, AAC_FRAME_SAMPLES),
			window[SHORT_START + 127 - m]);
	for (; m < AAC_FRAME_SAMPLES; m++)
		work[m] = 0;
}

/* 0x8b4150 */
void aacOverlapLong(int32_t *overlap, int32_t *work, const int32_t *window)
{
	int32_t m;

	for (m = 0; m < AAC_FRAME_SAMPLES; m++)
		work[m] = windowed(falling(overlap, m, AAC_FRAME_SAMPLES), window[1023 - m]);
}

/*
 * 0x8b4218: eight short windows; window 0 rises with the last frame's
 * shape. Samples 1024-1087, the next frame's first, are written past the
 * frame's end without the overlap.
 */
void aacWindowShort(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *previous, const int32_t *window)
{
	int32_t w, r, m;

	for (m = 0; m < 128; m++)
		output[SHORT_START + m] = aacAddSaturate(windowed(rising(spectrum, m, 128),
			previous[m]), work[SHORT_START + m]);
	for (w = 0; w < 4; w++) {
		for (r = 0; r < 128; r++) {
			int32_t n = SHORT_START + 128 + 128 * w + r;
			int32_t value = overlapped(spectrum + 128 * w + 32, spectrum + 128 * (w + 1),
				r, window);

			output[n] = n < AAC_FRAME_SAMPLES ? aacAddSaturate(value, work[n]) : value;
		}
	}
}

/* 0x8b4b30: zero, the short window's rise, then flat */
void aacWindowStop(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *window)
{
	int32_t m;

	for (m = SHORT_START; m < SHORT_START + 128; m++)
		output[m] = aacAddSaturate(windowed(rising(spectrum, m, AAC_FRAME_SAMPLES),
			window[m - SHORT_START]), work[m]);
	for (; m < AAC_FRAME_SAMPLES; m++)
		output[m] = aacAddSaturate(rising(spectrum, m, AAC_FRAME_SAMPLES), work[m]);
}

/* 0x8b4068 */
void aacWindowLong(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *window)
{
	int32_t m;

	for (m = 0; m < AAC_FRAME_SAMPLES; m++)
		output[m] = aacAddSaturate(windowed(rising(spectrum, m, AAC_FRAME_SAMPLES),
			window[m]), work[m]);
}
