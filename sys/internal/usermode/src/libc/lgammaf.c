/*
 * lgammaf function: the logarithm of |gamma(x)| in float. Large |x| take
 * Stirling's series (with the reflection formula for negative x), the
 * neighbourhoods of the zeros a polynomial about the zero, 1.2 to 1.5 a
 * ratio of polynomials, and the rest _FTgamma's scaled gamma.
 */
#include <math.h>

#include "xmath.h"

float _FGamma_big = 31.0F;	/* Stirling's series is exact enough above */

static const float c[] = {	/* Stirling's series in 1/x^2 */
	0.083333336F,
};
static const float num[] = {	/* between 1.2 and 1.5, a ratio of polynomials */
	0.000536076F, -0.0022131484F, 0.007719251F, -0.029474126F, 0.16490972F, 0.46540776F,
	-0.18796054F, -0.1152409F,
};
static const float den[] = {
	0.6424155F, 1.0F,
};

static const struct {	/* the ranges, each with its approximation */
	float xmax;	/* the range ends below xmax */
	float xoff;	/* the approximation's origin */
	const float *num;
	int nnum;	/* the numerator's degree */
	const float *den;
	int nden;	/* the denominator's */
} range[] = {
	{1.5F, 1.35F, num, 7, den, 1},
};

static const float z0[] = {
	1.6156843e+14F, 4.2725892e+10F, 12710959.0F, 5044.03F, -5.7432024e-12F,
};
static const float z1[] = {
	-4.262349e+10F, 12690641.0F, -5035.9673F, 5.313011e-14F,
};
static const float z2[] = {
	3.8421917e+27F, 5.9139633e+24F, 9.216653e+21F, 1.4591747e+19F, 2.3582843e+16F,
	3.920302e+13F, 6.788466e+10F, 125386984.0F, 260548.4F, 723.73663F, 8.31006e-12F,
};
static const float z3[] = {
	257851.6F, -716.2454F, 1.6712506e-12F,
};
static const float z4[] = {
	6.4980374e+17F, 6.007721e+15F, 5.642567e+13F, 5.410016e+11F, 5.335251e+09F,
	5.480749e+07F, 600556.2F, 7404.762F, 123.36218F, -7.6155456e-13F,
};
static const float z5[] = {
	4.784686e+15F, -4.62357e+13F, 4.5609606e+11F, -4.6277473e+09F, 48911584.0F,
	-5.5142e+05F, 6995.236F, -116.53578F, 3.2819732e-13F,
};
static const float z6[] = {
	3.680601e+29F, 1.5177406e+28F, 6.272812e+26F, 2.5990462e+25F, 1.0798676e+24F,
	4.5005903e+22F, 1.8822349e+21F, 7.902744e+19F, 3.332856e+18F, 1.4127867e+17F,
	6.024404e+15F, 2.5868905e+14F, 1.1200724e+13F, 4.898679e+11F, 2.1692365e+10F,
	9.7583014e+08F, 44812216.0F, 2116669.5F, 104145.555F, 5465.692F, 324.2553F, 26.790482F,
	1.8130149e-14F,
};
static const float z7[] = {
	-1.5577224e+11F, 7.834398e+09F, -4.00277e+08F, 20877114.0F, -1119991.5F, 62588.082F,
	-3730.6047F, 251.71468F, -20.725061F, -4.6433262e-14F,
};
static const float z8[] = {
	2.933994e+11F, 4.5135585e+10F, 6.9791155e+09F, 1.0855732e+09F, 1.7003734e+08F,
	26855526.0F, 4284377.5F, 692049.6F, 113559.664F, 19023.062F, 3277.1938F, 588.89075F,
	112.26899F, 25.831339F, 7.7818847F, -1.7579854e-15F,
};
static const float z9[] = {
	3.233548e+09F, -8.638733e+08F, 2.3159336e+08F, -62330684.0F, 16850484.0F, -4578716.5F,
	1251567.5F, -344497.6F, 95617.45F, -26804.578F, 7610.398F, -2193.1128F, 646.906F,
	-194.76616F, 62.62728F, -20.095135F, 9.57519F, -1.9143502F, -6.7679134e-16F,
};
static const float z10[] = {
	33630.457F, 18347.17F, 7776.158F, 4487.2183F, 1811.2054F, 1130.4579F, 425.17984F,
	296.4364F, 100.648125F, 82.22916F, 24.038822F, 24.824942F, 5.8004146F, 8.721783F,
	1.4112911F, 4.8583207F, 1.5156034F, -2.6360606e-15F,
};
static const float z11[] = {
	-0.03172183F, 0.21973324F, 0.8145158F, 0.46704757F,
};

static const struct {	/* the zeros, each with its Taylor polynomial */
	float lo;	/* the neighbourhood, lo < x < hi */
	float hi;
	float zhi;	/* the zero, as zhi + zlo */
	float zlo;
	const float *tab;
	int n;	/* the polynomial's degree */
} zero[] = {
	{-7.00023F, -7.00017F, -7.0001984F, 3.0850487e-08F, z0, 4},
	{-6.99988F, -6.99976F, -6.9998016F, 1.2785155e-07F, z1, 3},
	{-6.00176F, -6.00137F, -6.0013857F, 3.943286e-07F, z2, 10},
	{-5.9987F, -5.9985F, -5.9986076F, 1.5541717e-07F, z3, 2},
	{-5.01F, -5.0064F, -5.0082183F, 1.2009903e-07F, z4, 9},
	{-4.99266F, -4.991F, -4.9915447F, 8.295069e-08F, z5, 8},
	{-4.062F, -4.0255F, -4.039362F, 1.13994815e-07F, z6, 22},
	{-3.965F, -3.953F, -3.9552944F, 8.579265e-08F, z7, 9},
	{-3.1962F, -3.121F, -3.143581F, 2.519372e-08F, z8, 15},
	{-2.864F, -2.65F, -2.7476828F, 1.631023e-07F, z9, 18},
	{-2.65F, -2.265F, -2.4570248F, 7.447756e-08F, z10, 17},
	{2.72F, 2.77F, 2.7399998F, 2.2888183e-07F, z11, 3},
};

static const float half_ln2pi = 0.41893855F;	/* ln(2 pi) / 2 - 1/2 */
static const float ln2hi = 0.69314706F;	/* ln 2, high bits */
static const float ln2lo = 1.1730464e-07F;	/* ln 2, the rest */
static const float pi = 3.14159265358979323846F;

static float stirling(float x)	/* lgammaf(x) for x >= _FGamma_big */
{
	const float r = 1.0F / x;
	const float lnxm1 = _FLog(x, 0) - 1.0F;
	float y = x - 0.5F;
	short xexp;

	_FDunscale(&xexp, &y);
	y *= lnxm1;
	if (_FDscale(&y, xexp) < 0)
		y = c[0] * r + half_ln2pi + y;
	else
		_Feraise(_FE_OVERFLOW);
	return y;
}

float (lgammaf)(float x)
{
	float y = x;

	switch (_FDint(&y, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return _FInf._Float;
	case 0:
		if (x <= 0.0F) {	/* a pole at the integers not above zero */
			_Feraise(_FE_DIVBYZERO);
			return _FInf._Float;
		}
		/* fall through */
	default:
		if (x < -_FGamma_big) {	/* the reflection of a large -x */
			const float w = _FLog(pi / (x * _FSin((x - y) * pi, 0)), 0);

			return w - stirling(-x);
		} else if (_FGamma_big <= x) {
			return stirling(x);
		} else {
			int i;

			for (i = 0; i < sizeof(zero) / sizeof(zero[0]); ++i)
				if (zero[i].lo < x && x < zero[i].hi)
					break;
			if (i < sizeof(zero) / sizeof(zero[0]))	/* near a zero */
				return _FPoly(x - zero[i].zhi - zero[i].zlo, zero[i].tab, zero[i].n);
			if (1.2F < x && x < 1.5F) {
				for (i = 0; i < sizeof(range) / sizeof(range[0]); ++i)
					if (x < range[i].xmax)
						break;
				x -= range[i].xoff;
				return _FPoly(x, range[i].num, range[i].nnum)
				       / _FPoly(x, range[i].den, range[i].nden);
			} else {
				short xexp;
				const float z = _FTgamma(&x, &xexp);
				const float n = -xexp;

				x = -_FLog(x < 0.0F ? -x : x, 0) + n * ln2lo + n * ln2hi;
				return x + log1pf(z);
			}
		}
	}
}
