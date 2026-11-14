/*
 * erfl function: the error function in long double, a rational approximation in x^2 below
 * _LErf_small, 1 - _LErfc(|x|) up to _LErf_big, and +-1 beyond.
 */
#include <math.h>

#include "xmath.h"

long double _LErf_big = 5.84L;	/* erfc(x) is lost in 1 above */
long double _LErf_small = 1.523L;	/* the rational approximation's range */

static const long double num[] = {	/* the numerator's coefficients, highest first */
	1.2040999373658753e-07, 8.878440561535706e-06, 0.0003280663971387854,
	0.0034814902101731196, 0.05172838573198753, 0.1764401132412738,
	1.1283791670955126,
};
static const long double den[] = {	/* the denominator's */
	1.4060567875468972e-06, 5.841103288981175e-05, 0.0011742377551152094,
	0.014283713727543943, 0.10907619238514397, 0.48969931241768294,
	1.0,
};

long double (erfl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return LSIGN(x) ? -1.0L : 1.0L;
	default: {
		long double y = x < 0.0L ? -x : x;

		if (y < _LErf_small) {
			const long double g = x * x;

			return ((((((num[0] * g + num[1]) * g + num[2]) * g + num[3]) * g + num[4]) * g
				 + num[5]) * g + num[6])
			       / ((((((den[0] * g + den[1]) * g + den[2]) * g + den[3]) * g + den[4]) * g
				  + den[5]) * g + den[6])
			       * x;
		} else {
			long double z = 1.0L;

			if (y < _LErf_big)
				z -= _LErfc(y);
			return x < 0.0L ? -z : z;
		}
	}
	}
}
