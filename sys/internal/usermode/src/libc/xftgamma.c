/*
 * _FTgamma function: for tgammaf, gamma(*px) as (1 + the return value)
 * divided by *px 2^*pex. x is moved to [-0.5, 0.5] by the recurrence
 * gamma(x + 1) = x gamma(x), with the factors in *px, and gamma(x + 2) - 1
 * returned from a rational approximation; from 31 up, the reciprocal of
 * Stirling's series is put in *px and 0 returned.
 */
#include <math.h>

#include "xmath.h"

static const float p[] = {	/* gamma(x + 2) - 1 is x p(x) / q(x) */
	0.013244764F, 0.0094383843F, 0.4292531F, 0.4329604F,
};
static const float q[] = {
	0.035618942F, -0.19255313F, 0.01774013F, 1.0240692F,
};
static const float stirnum[] = {	/* sqrt(2pi) times Stirling's series, in 1/x - xoff */
	-0.001481641F, 0.0003563276F, 0.0018985819F, -0.0018186223F,
	-0.004966273F, 0.05404323F, 0.75926095F, 2.5116735F,
};
static const float stirden[] = {
	0.21896438F, 1.0F,
};
static const struct {	/* the ranges of x above 31, each with its approximation */
	float xmax;	/* the range ends below xmax */
	float xoff;	/* the middle of 1/x in the range */
	const float *num;
	int nnum;	/* the numerator's degree */
	const float *den;
	int nden;	/* the denominator's */
} range[] = {
	{67.0F, 0.024129353F, stirnum, 7, stirden, 1},
};

float _FTgamma(float *px, short *pex)
{
	float x = *px;
	float z = 1.0F;

	if (31.0F <= x) {	/* Stirling: x^(x - 1/2) e^-x sqrt(2pi) times the series */
		float y;

		z = _FPow(x, x - 0.5F, pex);
		y = 1.0F / x - range[0].xoff;
		*pex = -*pex;
		*px = _FPoly(y, range[0].den, range[0].nden) / _FPoly(y, range[0].num, range[0].nnum);
		*px /= z * expf(-x);
		return 0.0F;
	} else if (x < -0.5F) {	/* multiply up */
		for (;;) {
			z *= x;
			if (0.5F < x)
				break;
			x += 1.0F;
		}
		x -= 1.0F;
		_FDunscale(pex, &z);
	} else if (x < 0.5F) {
		z = x * (x + 1.0F);
		_FDunscale(pex, &z);
	} else if (x < 1.5F) {
		z = x;
		*pex = 0;
		x -= 1.0F;
	} else if (x < 2.5F) {
		z = 1.0F;
		*pex = 0;
		x -= 2.0F;
	} else {	/* divide down */
		z = 1.0F;
		while (2.5F < x) {
			x -= 1.0F;
			z /= x;
		}
		x -= 2.0F;
		_FDunscale(pex, &z);
	}
	*px = z;
	return (((p[0] * x + p[1]) * x + p[2]) * x + p[3])
	       / (((q[0] * x + q[1]) * x + q[2]) * x + q[3]) * x;
}
