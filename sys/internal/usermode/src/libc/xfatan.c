/*
 * _FAtan function: the arctangent of x in [0, 1] in float, reduced by pi/6
 * above 2 - sqrt(3) and approximated by a rational function. idx picks the
 * result: bit 1 negates, bits 0-2 add the multiple of pi/6 in addend[], bit
 * 3 negates the sum.
 */
#include "xmath.h"

static const float denom[] = {	/* the denominator's coefficients, highest first */
	0.08280214F, 0.8483081F,
};
static const float numer[] = {	/* the numerator's */
	0.5149752F,
};
static const float addend[] = {	/* 0, pi/6, pi/2, pi/3, pi/2, 2pi/3, pi, 5pi/6 */
	0.0F, 0.5235988F, 1.5707964F, 1.0471976F,
	1.5707964F, 2.0943952F, 3.1415927F, 2.6179938F,
};
static const float sqrt3 = 1.73205080756887729353F;
static const float twomsqrt3 = 0.26794919243112270647F;	/* 2 - sqrt(3) */

float _FAtan(float x, int idx)
{
	if (twomsqrt3 < x) {	/* reduce by pi/6 */
		x = (sqrt3 * x - 1.0F) / (x + sqrt3);
		idx |= 1;
	}
	if (x < -_FRteps._Float || _FRteps._Float < x) {	/* x^3 matters */
		const float g = x * x;

		x = (x + x * g * numer[0]) / (1.0F + g * (denom[0] * g + denom[1]));
	}
	if (idx & 2)
		x = -x;
	if (idx & 7) {
		x += addend[idx & 7];
		_Feraise(_FE_INEXACT);
	}
	return idx & 8 ? -x : x;
}
