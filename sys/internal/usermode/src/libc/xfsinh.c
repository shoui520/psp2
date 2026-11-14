/*
 * _FSinh function: y times the hyperbolic sine of x, in float, for sinhf
 * and the complex functions.
 */
#include "xmath.h"

static const float q[] = {	/* below ln3/2: the denominator, 1 + x^2 times this */
	0.001839977F, -0.061035976F,
};
static const float p[] = {	/* the numerator, x + x^3 times this */
	0.10563073F,
};
static const float ln3by2 = 0.54930615F;

float _FSinh(float x, float y)
{
	const short errx = _FDtest(&x);
	const short erry = _FDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			return x;
		} else if (erry == _NANCODE) {
			return y;
		} else if (errx == _INFCODE) {
			if (erry != 0)
				return x * y;
			_Feraise(_FE_INVALID);	/* INF times 0 */
			return _FNan._Float;
		} else if (errx == 0) {
			if (erry != _INFCODE)
				return x * y;
			_Feraise(_FE_INVALID);	/* 0 times INF */
			return _FNan._Float;
		} else {
			return x * y;
		}
	} else {	/* finite */
		int neg = 0;

		if (x < 0.0F) {
			x = -x;
			neg = 1;
		}
		if (x < _FRteps._Float) {	/* tiny: y x */
			x = x * y;
		} else if (x < ln3by2) {	/* a rational approximation */
			const float w = x * x;

			x = y * ((x + x * w * p[0]) / (1.0F + w * (q[0] * w + q[1])));
		} else if (x < _FXbig) {	/* y (e^x - e^-x) / 2 */
			_FExp(&x, 1.0F, -1);
			x = y * (x - 0.25F / x);
		} else {	/* y e^x / 2 */
			_FExp(&x, y, -1);
		}
		return neg ? -x : x;
	}
}
