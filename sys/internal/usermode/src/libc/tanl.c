/*
 * tanl function: the tangent of x, a rational approximation on x reduced
 * to [-pi/4, pi/4], inverted in the odd quadrants.
 */
#include <math.h>

#include "xmath.h"

static const long double den[] = {	/* the denominator's coefficients, highest first */
	-0.0002084495638225882L, 0.023344894646932932L, -0.461616897689962L,
};
static const long double num[] = {	/* the numerator's */
	-7.48373924372997e-06L, 0.0028059287503523307L, -0.1282835643566316L,
};

long double (tanl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:	/* a domain error */
		_Feraise(_FE_INVALID);
		return _LNan._Long_double;
	case 0:
		return x;
	default: {	/* finite */
		const unsigned int qoff = _LQuad(&x);

		if (-_LRteps._Long_double < x && x < _LRteps._Long_double) {	/* tan(x) is x */
			if ((qoff & 1) == 0) {
				return x;
			} else {	/* -1/x, scaled to keep 1/x in range */
				short xexp;

				_LDunscale(&xexp, &x);
				x = -1.0L / x;
				if (0 < _LDscale(&x, -xexp))
					_Feraise(_FE_OVERFLOW);
				return x;
			}
		} else {
			const long double g = x * x;
			const long double gd = ((den[0] * g + den[1]) * g + den[2]) * g + 1.0L;

			x += x * g * ((num[0] * g + num[1]) * g + num[2]);
			return (qoff & 1) != 0 ? -gd / x : x / gd;
		}
	}
	}
}
