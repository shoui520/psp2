/*
 * The inverse DCTs (VLIW): the tangent-rotation IDCT of the encoder's FDCT
 * run backwards, columns first, in 16-bit saturating arithmetic (rotations
 * Q14). The coefficients are in natural order, dequantised by a table the
 * decoder scaled for the IDCT ((c q) >> 6, saturated), and the samples come
 * out as (y + 1024) >> 3, clamped to 0-255. The reduced sizes take the top
 * left 4x4 or 2x2 coefficients.
 */
#include "../ivc2.h"
#include "jpeg.h"

#define TAN_1_16	3259	/* tan(pi/16), Q14 */
#define TAN_2_16	6786	/* tan(pi/8) */
#define TAN_3_16	10947	/* tan(3pi/16) */
#define COS_4_16	11585	/* cos(pi/4) */
#define LEVEL		1024	/* 128 << 3 */

static int32_t add(int32_t a, int32_t b)
{
	return ivc2Clip(a + b, 16);
}

static int32_t subtract(int32_t a, int32_t b)
{
	return ivc2Clip(a - b, 16);
}

/* (c x) >> 14, saturated */
static int32_t rotate(int32_t c, int32_t x)
{
	return ivc2Clip((c * x) >> 14, 16);
}

static int32_t dequantize(int16_t coef, uint16_t quant)
{
	return ivc2Clip((coef * (int16_t)quant) >> 6, 16);
}

static uint8_t sample(int32_t y)
{
	y = (y + LEVEL) >> 3;
	return y < 0 ? 0 : y > 255 ? 255 : y;
}

/* eight values `step` apart, in place */
static void idct8(int16_t *x, int32_t step)
{
	int32_t x0 = x[0], x1 = x[step], x2 = x[2 * step], x3 = x[3 * step];
	int32_t x4 = x[4 * step], x5 = x[5 * step], x6 = x[6 * step], x7 = x[7 * step];
	int32_t sum04 = add(x0, x4), difference04 = subtract(x0, x4);
	int32_t even2 = add(x2, rotate(TAN_2_16, x6)), even3 = subtract(rotate(TAN_2_16, x2), x6);
	int32_t a = add(sum04, even2), b = subtract(sum04, even2);
	int32_t c = add(difference04, even3), d = subtract(difference04, even3);
	int32_t p = add(x1, rotate(TAN_1_16, x7)), q = add(rotate(TAN_3_16, x5), x3);
	int32_t r = subtract(rotate(TAN_1_16, x1), x7), s = subtract(x5, rotate(TAN_3_16, x3));
	int32_t pq = add(p, q), rs = add(r, s);
	int32_t m1 = rotate(COS_4_16, add(subtract(p, q), subtract(r, s)));
	int32_t m2 = rotate(COS_4_16, subtract(subtract(p, q), subtract(r, s)));

	x[0] = add(a, pq);
	x[7 * step] = subtract(a, pq);
	x[step] = add(m1, c);
	x[6 * step] = subtract(c, m1);
	x[2 * step] = add(m2, d);
	x[5 * step] = subtract(d, m2);
	x[3 * step] = add(b, rs);
	x[4 * step] = subtract(b, rs);
}

/* four values `step` apart, in place */
static void idct4(int16_t *x, int32_t step)
{
	int32_t x0 = x[0], x1 = x[step], x2 = x[2 * step], x3 = x[3 * step];
	int32_t even0 = add(x0, x2), even1 = subtract(x0, x2);
	int32_t odd0 = add(rotate(TAN_2_16, x3), x1), odd1 = subtract(rotate(TAN_2_16, x1), x3);

	x[0] = add(even0, odd0);
	x[step] = add(even1, odd1);
	x[2 * step] = subtract(even1, odd1);
	x[3 * step] = subtract(even0, odd0);
}

/* inlined in the scan rows */
void jpegIdct8x8(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch)
{
	int16_t x[8][8];
	int32_t u, v;

	for (u = 0; u < 8; u++)
		for (v = 0; v < 8; v++)
			x[u][v] = dequantize(coef[u * 8 + v], quant[u * 8 + v]);
	for (v = 0; v < 8; v++)
		idct8(&x[0][v], 8);
	for (u = 0; u < 8; u++, out += pitch) {
		idct8(x[u], 1);
		for (v = 0; v < 8; v++)
			out[v] = sample(x[u][v]);
	}
}

/* 0x889cc0 */
void jpegIdct4x4(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch)
{
	int16_t x[4][4];
	int32_t u, v;

	for (u = 0; u < 4; u++)
		for (v = 0; v < 4; v++)
			x[u][v] = dequantize(coef[u * 8 + v], quant[u * 8 + v]);
	for (v = 0; v < 4; v++)
		idct4(&x[0][v], 4);
	for (u = 0; u < 4; u++, out += pitch) {
		idct4(x[u], 1);
		for (v = 0; v < 4; v++)
			out[v] = sample(x[u][v]);
	}
}

/* 0x889e60: here the products are only truncated to 16 bits, and the level added saturating */
void jpegIdct2x2(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch)
{
	int32_t x00 = (int16_t)(coef[0] * quant[0]), x01 = (int16_t)(coef[1] * quant[1]);
	int32_t x10 = (int16_t)(coef[8] * quant[8]), x11 = (int16_t)(coef[9] * quant[9]);
	int32_t sum0 = add(x00, x10), sum1 = add(x01, x11);
	int32_t difference0 = subtract(x00, x10), difference1 = subtract(x01, x11);
	int32_t y[2][2] = {
		{ add(sum0, sum1), subtract(sum0, sum1) },
		{ add(difference0, difference1), subtract(difference0, difference1) },
	};
	int32_t u, v, value;

	for (u = 0; u < 2; u++, out += pitch) {
		for (v = 0; v < 2; v++) {
			value = add(y[u][v], LEVEL) >> 3;
			out[v] = value < 0 ? 0 : value > 255 ? 255 : value;
		}
	}
}
