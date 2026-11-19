/*
 * The packet loss concealment's DSP loops (include/robin/sbc.h), ARM code in
 * the original: correlations, energies, scaled copies and sums of
 * magnitudes of 16-bit samples. The sums wrap as the original's
 * multiply-accumulates do.
 */
#include <stdint.h>
#include "robin/sbc.h"

/* The low and the high sample of a word of two. */
#define SBC_LO(word)	((int16_t)(word))
#define SBC_HI(word)	((word) >> 16)

/*
 * One pattern word x (two samples) against the signal at the lags 0..3: w0 and w1 hold the
 * four samples the lags start at, w2 is read for the fifth, and the next pattern word is read
 * into y ahead of its step.
 */
#define SBC_CORRELATE(w0, w1, w2, x, y)						\
	do {									\
		w2 = *s++;							\
		y = *p++;							\
		sum0 += (uint32_t)(SBC_LO(w0) * SBC_LO(x));			\
		sum1 += (uint32_t)(SBC_HI(w0) * SBC_LO(x));			\
		sum2 += (uint32_t)(SBC_LO(w1) * SBC_LO(x));			\
		sum3 += (uint32_t)(SBC_HI(w1) * SBC_LO(x));			\
		sum0 += (uint32_t)(SBC_HI(w0) * SBC_HI(x));			\
		sum1 += (uint32_t)(SBC_LO(w1) * SBC_HI(x));			\
		sum2 += (uint32_t)(SBC_HI(w1) * SBC_HI(x));			\
		sum3 += (uint32_t)(SBC_LO(w2) * SBC_HI(x));			\
	} while (0)

/*
 * The correlations of the pattern with the signal at the lags 0..3, in blocks of 16 samples
 * read two at a time; the signal and the pattern are both read one word ahead.
 */
void btSbcBuildPacket(const int16_t *signal, const int16_t *pattern, int32_t correlations[4], int count)
{
	const int32_t *s = (const int32_t *)signal;
	const int32_t *p = (const int32_t *)pattern;
	uint32_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
	int32_t a, b, c, d, x, y;

	a = *s++;
	b = *s++;
	x = *p++;
	do {
		SBC_CORRELATE(a, b, c, x, y);
		SBC_CORRELATE(b, c, d, y, x);
		SBC_CORRELATE(c, d, a, x, y);
		SBC_CORRELATE(d, a, b, y, x);
		SBC_CORRELATE(a, b, c, x, y);
		SBC_CORRELATE(b, c, d, y, x);
		SBC_CORRELATE(c, d, a, x, y);
		SBC_CORRELATE(d, a, b, y, x);
		count -= 16;
	} while (count > 0);
	correlations[0] = sum0;
	correlations[1] = sum1;
	correlations[2] = sum2;
	correlations[3] = sum3;
}

/* Copies samples shifted right by shift, or left by -shift. */
void btSbcComputePacket(const int16_t *input, int16_t *output, int shift, int count)
{
	int n;
	int16_t a, b, c, d;

	if (shift >= 0) {
		for (n = count >> 2; n != 0; n--) {
			a = *input++;
			b = *input++;
			c = *input++;
			d = *input++;
			*output++ = a >> shift;
			*output++ = b >> shift;
			*output++ = c >> shift;
			*output++ = d >> shift;
		}
		for (n = count & 3; n != 0; n--)
			*output++ = *input++ >> shift;
	} else {
		shift = -shift;
		for (n = count >> 2; n != 0; n--) {
			a = *input++;
			b = *input++;
			c = *input++;
			d = *input++;
			*output++ = a << shift;
			*output++ = b << shift;
			*output++ = c << shift;
			*output++ = d << shift;
		}
		for (n = count & 3; n != 0; n--)
			*output++ = *input++ << shift;
	}
}

void btSbcQueryPacket(const int16_t *input, int16_t *output, int count)
{
	int n;
	int16_t a, b, c, d;

	n = count >> 2;
	if (n != 0) {
		do {
			a = *input++;
			b = *input++;
			c = *input++;
			d = *input++;
			*output++ = a;
			*output++ = b;
			*output++ = c;
			*output++ = d;
		} while (--n != 0);
	}
	n = count - ((count >> 2) << 2);
	if (n > 0) {
		do
			*output++ = *input++;
		while (--n != 0);
	}
}

/* The energy of 64 samples, each a quarter. */
uint32_t btSbcResetPacket(const int16_t *signal)
{
	uint32_t sum = 0;
	int n;
	int16_t a, b, c, d;

	for (n = SBC_PLC_TEMPLATE / 4; n != 0; n--) {
		a = *signal++ >> 2;
		b = *signal++ >> 2;
		c = *signal++ >> 2;
		d = *signal++ >> 2;
		sum += a * a;
		sum += b * b;
		sum += c * c;
		sum += d * d;
	}
	return sum;
}

/* The energy of 64 samples. */
uint32_t btSbcSyncPacket(const int16_t *signal)
{
	const int32_t *pairs = (const int32_t *)signal;
	uint32_t sum = 0;
	int n;
	int32_t a, b;

	for (n = SBC_PLC_TEMPLATE / 4; n != 0; n--) {
		a = *pairs++;
		b = *pairs++;
		sum += (int16_t)a * (int16_t)a;
		sum += (a >> 16) * (a >> 16);
		sum += (int16_t)b * (int16_t)b;
		sum += (b >> 16) * (b >> 16);
	}
	return sum;
}

/* The sums of the magnitudes of count samples, two at a time, going back from first and second. */
void btSbcApplyPacket(const int16_t *first, const int16_t *second, int count, int32_t *firstSum,
		      int32_t *secondSum)
{
	uint32_t sumFirst = 0, sumSecond = 0;
	int n, a, b, c, d;

	n = count >> 1;
	if (n > 0) {
		do {
			a = *first--;
			b = *first--;
			c = *second--;
			d = *second--;
			sumFirst += a > 0 ? a : -a;
			sumFirst += b > 0 ? b : -b;
			sumSecond += c > 0 ? c : -c;
			sumSecond += d > 0 ? d : -d;
		} while (--n != 0);
	}
	*firstSum = sumFirst;
	*secondSum = sumSecond;
}
