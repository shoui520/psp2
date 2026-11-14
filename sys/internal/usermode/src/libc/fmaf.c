/*
 * fmaf function: x * y + z rounded once, in float. Where the product and z nearly
 * cancel (opposite signs, exponents within 2), the sum is formed in
 * extended precision; otherwise the plain expression is exact enough.
 */
#include <math.h>

#include "xmath.h"

float (fmaf)(float x, float y, float z)
{
	short xexp;
	short yexp;
	short zexp;
	float xu = x;
	float yu = y;
	float zu = z;
	short errx;
	short erry;
	short errz;

	if ((errx = _FDunscale(&xexp, &xu)) == _NANCODE)
		return x;
	if ((erry = _FDunscale(&yexp, &yu)) == _NANCODE)
		return y;
	if ((errz = _FDunscale(&zexp, &zu)) == _NANCODE) {
		return z;
	} else if (errx == _INFCODE || erry == _INFCODE) {	/* an infinite product */
		if (errx != 0 && erry != 0
		    && (errz != _INFCODE || (FSIGN(x) ^ FSIGN(y)) == FSIGN(z)))
			return x * y + z;
		_Feraise(_FE_INVALID);	/* 0 * inf, or inf - inf */
		return _FNan._Float;
	} else if (errz == _INFCODE) {
		return z;
	} else if (errx == 0 || erry == 0 || errz == 0) {
		return x * y + z;
	} else {
		const long xyexp = xexp + yexp;
		const short n = zexp - xyexp;
		float xp[4];
		float yp[2];
		float zp[2];

		if ((FSIGN(x) ^ FSIGN(y) ^ FSIGN(z)) == 0 || n < -2 || 2 < n)
			return x * y + z;
		zexp = n;
		_FDscale(&zu, n);	/* z relative to the product */
		_FXp_setw(xp, 4, xu);
		_FXp_setw(yp, 2, yu);
		_FXp_setw(zp, 2, zu);
		_FXp_mulh(xp, 4, yp[0]);
		if (yp[1] != 0.0F) {
			float tp[4];

			_FXp_setw(tp, 4, xu);
			_FXp_mulh(tp, 4, yp[1]);
			_FXp_addx(xp, 4, tp, 4);
		}
		_FXp_addx(xp, 4, zp, 2);
		zu = _FXp_getw(xp, 4);
		_FDscale(&zu, xyexp);
		return zu;
	}
}
