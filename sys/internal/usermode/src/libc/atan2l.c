/*
 * atan2l function: the arctangent, in long double, of y / x in the quadrant of (x, y), through
 * _LAtan of the smaller over the larger magnitude.
 */
#include <math.h>

#include "xmath.h"

long double (atan2l)(long double y, long double x)
{
	long double z;
	const short errx = _LDtest(&x);
	const short erry = _LDtest(&y);
	int idx;

	if (errx <= 0 && erry <= 0) {	/* x and y finite or zero */
		if (LSIGN(y)) {
			y = -y;
			idx = 8;
		} else {
			idx = 0;
		}
		if (LSIGN(x)) {
			x = -x;
			idx ^= 6;
		}
		if (x < y) {
			z = x / y;
			idx ^= 2;
		} else {
			z = errx < 0 ? y / x : 0.0L;
		}
	} else if (erry == _NANCODE) {
		return y;
	} else if (errx == _NANCODE) {
		return x;
	} else {	/* at least one infinity */
		z = errx == erry ? 1.0L : 0.0L;
		idx = LSIGN(y) ? 8 : 0;
		if (LSIGN(x))
			idx ^= 6;
		if (erry == _INFCODE)
			idx ^= 2;
	}
	return _LAtan(z, idx);
}
