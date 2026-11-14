/*
 * tan function: the tangent of x, a rational approximation on x reduced
 * to [-pi/4, pi/4], inverted in the odd quadrants.
 */
#include <math.h>

#include "xmath.h"

static const double den[] = {	/* the denominator's coefficients, highest first */
	-0.0002084495638225882, 0.023344894646932932, -0.461616897689962,
};
static const double num[] = {	/* the numerator's */
	-7.48373924372997e-06, 0.0028059287503523307, -0.1282835643566316,
};

double (tan)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:	/* a domain error */
		_Feraise(_FE_INVALID);
		return _Nan._Double;
	case 0:
		return x;
	default: {	/* finite */
		const unsigned int qoff = _Quad(&x);

		if (-_Rteps._Double < x && x < _Rteps._Double) {	/* tan(x) is x */
			if ((qoff & 1) == 0) {
				return x;
			} else {	/* -1/x, scaled to keep 1/x in range */
				short xexp;

				_Dunscale(&xexp, &x);
				x = -1.0 / x;
				if (0 < _Dscale(&x, -xexp))
					_Feraise(_FE_OVERFLOW);
				return x;
			}
		} else {
			const double g = x * x;
			const double gd = ((den[0] * g + den[1]) * g + den[2]) * g + 1.0;

			x += x * g * ((num[0] * g + num[1]) * g + num[2]);
			return (qoff & 1) != 0 ? -gd / x : x / gd;
		}
	}
	}
}
