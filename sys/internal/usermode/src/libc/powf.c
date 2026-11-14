/*
 * powf function, and _FPow: x^y as 2^(y log2(x)) by _FExp. x is split into
 * 2^xexp times a mantissa between sqrt(0.5) and sqrt(2), whose log2 comes
 * from the series for ln((1 + z) / (1 - z)). When y log2(x) is beyond +-12
 * (or |y| beyond 40), the product is formed in extended precision by the
 * _FXp_ functions. _FPow returns the binary exponent in *pex when pex is not
 * null.
 */
#include <math.h>
#include <string.h>

#include "xmath.h"

#define NBUF	4	/* the extended-precision words */

/* the sign bit of a value, from its bits */
#define SIGN(x)	(((union { float _V; unsigned int _W; }){(x)})._W & 0x80000000U)

static const float invln2[NBUF] = {	/* 1 / ln(2), in four parts */
	1.4423828F, 0.00031220913F, 1.9237632e-08F, 2.1991298e-11F,
};

float _FPow(float x, float y, short *pex)
{
	static const float ln2 = 0.69314718F;
	short xexp;
	float yi = y;
	float z;
	const short errx = _FDunscale(&xexp, &x);
	const short erry = _FDint(&yi, 0);
	float xr;
	int n;
	short neg;

	if (pex != 0)
		*pex = 0;
	if ((erry == 0 && y == 0.0F)
	    || (errx < 0 && xexp == 1 && (x == 0.5F || (erry == _INFCODE && x == -0.5F))))
		return 1.0F;	/* x^0, 1^y, -1^INF */
	if (0 <= errx || 0 < erry) {	/* x is 0, INF or NaN, or y is INF or NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx == _INFCODE)	/* x == +-INF */
			return !SIGN(x) ? (!SIGN(y) ? _FInf._Float : 0.0F)
				: !SIGN(y) ? (erry == 0 && _FDint(&yi, -1) < 0
					? -_FInf._Float : _FInf._Float)
				: erry == 0 && _FDint(&yi, -1) < 0 ? -_FZero : 0.0F;
		else if (erry == _INFCODE)	/* y == +-INF */
			return !SIGN(y) ? (0 < xexp ? _FInf._Float : 0.0F)
				: 0 < xexp ? 0.0F : _FInf._Float;
		else if (!SIGN(y))	/* x == 0, y > 0 */
			return erry == 0 && _FDint(&yi, -1) < 0 && SIGN(x) ? -_FZero : 0.0F;
		_Feraise(_FE_DIVBYZERO);	/* x == 0, y < 0 */
		return erry == 0 && _FDint(&yi, -1) < 0 && SIGN(x)
			? -_FInf._Float : _FInf._Float;
	}

	if (SIGN(x) && erry < 0) {	/* negative x, nonintegral y */
		_Feraise(_FE_INVALID);
		return _FNan._Float;
	}
	neg = 0;
	if (0.0F < x) {
		;
	} else {	/* odd y makes the result negative */
		x = -x;
		neg = _FDint(&yi, -1);
	}
	if (x < 0.70710677F) {	/* below sqrt(0.5): use 2x */
		--xexp;
		x *= 2.0F;
	}
	{
		const float w = (x - 1.0F) / (x + 1.0F);
		const float w2 = w * w;
		float series;

		x -= 1.0F;
		series = w * (w2 * _FLogpoly(w2) - x);
		yi = (x + series) / ln2;
		yi = xexp + yi;
		z = y * yi;
		if (z < -32768.0F)
			goto underflow;
		if (-12.0F <= z && z <= 12.0F && -40.0F < y && y < 40.0F) {	/* in float */
			xr = z < 0.0F ? z - 0.5F : z + 0.5F;
			z = y * (x + series) + (y * xexp - (int)xr) * ln2;
		} else if (32768.0F < z) {
			goto overflow;
		} else {	/* in extended precision */
			float ws[2] = {0.0F};
			float wx[NBUF] = {0.0F};
			float wl[NBUF] = {0.0F};
			float wz[NBUF] = {0.0F};
			int i;

			_FXp_setw(wx, NBUF, x);
			_FXp_setw(ws, 2, series);
			_FXp_addx(wx, NBUF, ws, 2);	/* ln(x) */
			if (wx[0] == 0.0F) {
				_FXp_setw(wl, NBUF, 0.0F);
			} else {	/* times 1 / ln(2) */
				memcpy(wl, invln2, sizeof (wl));
				_FXp_mulh(wl, NBUF, wx[0]);
				for (i = 1; i < NBUF && wx[i] != 0.0F; ++i) {
					float wt[NBUF];

					memcpy(wt, invln2, sizeof (wt));
					_FXp_mulh(wt, NBUF, wx[i]);
					_FXp_addx(wl, NBUF, wt, NBUF);
				}
			}
			_FXp_addh(wl, NBUF, xexp);	/* log2(x) */
			_FXp_setw(wx, 2, y);
			memcpy(wz, wl, sizeof (wz));
			_FXp_mulh(wz, NBUF, wx[0]);
			if (wx[1] != 0.0F) {
				float wt[NBUF];

				memcpy(wt, wl, sizeof (wt));
				_FXp_mulh(wt, NBUF, wx[1]);
				_FXp_addx(wz, NBUF, wt, NBUF);
			}
			x = wz[0] + wz[1] + wz[2];	/* the integer part of y log2(x) */
			_FDint(&x, 0);
			_FXp_addh(wz, NBUF, -x);
			z = _FXp_getw(wz, NBUF) * ln2;
			xr = x;
		}
	}
	n = (int)xr;
	if (pex != 0) {
		*pex = n;
		n = 0;
	}
	switch (_FExp(&z, 1.0F, n)) {
	case 0:
underflow:
		z = 0.0F;
		_Feraise(_FE_UNDERFLOW);
		break;
	case _INFCODE:
overflow:
		z = _FInf._Float;
		_Feraise(_FE_OVERFLOW);
		break;
	}
	return neg ? -z : z;
}

float powf(float x, float y)
{
	return _FPow(x, y, 0);
}
