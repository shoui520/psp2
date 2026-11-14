/*
 * tanf function: the tangent of x, a rational approximation on x reduced
 * to [-pi/4, pi/4], inverted in the odd quadrants.
 */
#include <math.h>

#include "xmath.h"

static const float den[] = {	/* the denominator's coefficients, highest first */
	0.009709937F, -0.4291162F,
};
static const float num[] = {	/* the numerator's */
	-0.0957823F,
};

float (tanf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:	/* a domain error */
		_Feraise(_FE_INVALID);
		return _FNan._Float;
	case 0:
		return x;
	default: {	/* finite */
		const unsigned int qoff = _FQuad(&x);

		if (-_FRteps._Float < x && x < _FRteps._Float) {	/* tan(x) is x */
			if ((qoff & 1) == 0) {
				return x;
			} else {	/* -1/x, scaled to keep 1/x in range */
				short xexp;

				_FDunscale(&xexp, &x);
				x = -1.0F / x;
				if (0 < _FDscale(&x, -xexp))
					_Feraise(_FE_OVERFLOW);
				return x;
			}
		} else {
			const float g = x * x;
			const float gd = (den[0] * g + den[1]) * g + 1.0F;

			x += x * g * num[0];
			return (qoff & 1) != 0 ? -gd / x : x / gd;
		}
	}
	}
}
