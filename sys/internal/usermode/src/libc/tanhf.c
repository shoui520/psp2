/*
 * tanhf function: the hyperbolic tangent of x, a rational approximation
 * below ln 2 (of x/2 doubled below ln 2), 1 - 2/(e^2x + 1) up to xbig, 1
 * beyond.
 */
#include <math.h>

#include "xmath.h"

static const float xbig = 8.32F;	/* tanh(x) is 1 in float from here */
static const float den[] = {	/* the denominator's coefficients, highest first */
	0.3996813F,
};
static const float num[] = {	/* the numerator's */
	0.06635193F,
};
static const float ln2by2 = 0.34657359027997265471F;	/* ln 2 / 2 */
static const float ln2 = 0.69314718055994530942F;

float (tanhf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return FSIGN(x) ? -1.0F : 1.0F;
	case 0:
		return x;
	default: {	/* finite */
		float g;
		int neg = 0;

		if (x < 0.0F) {
			x = -x;
			neg = 1;
		}
		if (x < _FRteps._Float) {	/* tanh(x) is x */
			;
		} else if (x < ln2by2) {
			g = x * x;
			x = (x + x * g * num[0]) / (g * den[0] + 1.0F);
		} else if (x < ln2) {	/* tanh(x) = 2 tanh(x/2) / (1 + tanh(x/2)^2) */
			x *= 0.5F;
			g = x * x;
			x = (x + x * g * num[0]) / (g * den[0] + 1.0F);
			x = x * 2.0F / (x * x + 1.0F);
		} else if (x < xbig) {
			x = 1.0F - 2.0F / (expf(x + x) + 1.0F);
		} else {
			x = 1.0F;
		}
		return neg ? -x : x;
	}
	}
}
