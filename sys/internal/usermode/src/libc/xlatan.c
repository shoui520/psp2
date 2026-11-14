/*
 * _LAtan function: the arctangent of x in [0, 1], reduced by pi/6 above
 * 2 - sqrt(3) and approximated by a rational function. idx picks the result:
 * bit 1 negates, bits 0-2 add the multiple of pi/6 in addend[], bit 3
 * negates the sum.
 */
#include "xmath.h"

static const long double denom[] = {	/* the denominator's coefficients, highest first */
	0.0050666613825698075L, 0.1878662719730893L, 1.0529079768725262L, 1.8489609299024357L,
};
static const long double numer[] = {	/* the numerator's */
	0.06383198950003616L, 0.6365876669047934L, 1.5156275965691033L,
};
static const long double addend[] = {	/* 0, pi/6, pi/2, pi/3, pi/2, 2pi/3, pi, 5pi/6 */
	0.0L, 0.5235987755982989L, 1.5707963267948966L, 1.0471975511965979L,
	1.5707963267948966L, 2.0943951023931957L, 3.141592653589793L, 2.6179938779914944L,
};
static const long double sqrt3 = 1.73205080756887729353L;
static const long double twomsqrt3 = 0.26794919243112270647L;	/* 2 - sqrt(3) */

long double _LAtan(long double x, int idx)
{
	if (twomsqrt3 < x) {	/* reduce by pi/6 */
		x = (sqrt3 * x - 1.0L) / (x + sqrt3);
		idx |= 1;
	}
	if (x < -_LRteps._Long_double || _LRteps._Long_double < x) {	/* x^3 matters */
		const long double g = x * x;

		x = (x + x * g * ((numer[0] * g + numer[1]) * g + numer[2]))
		    / (1.0L + g * (((denom[0] * g + denom[1]) * g + denom[2]) * g + denom[3]));
	}
	if (idx & 2)
		x = -x;
	if (idx & 7) {
		x += addend[idx & 7];
		_Feraise(_FE_INEXACT);
	}
	return idx & 8 ? -x : x;
}
