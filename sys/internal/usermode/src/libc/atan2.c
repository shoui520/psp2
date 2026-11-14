/*
 * atan2 function: the arctangent of y / x in the quadrant of (x, y), through
 * _Atan of the smaller over the larger magnitude.
 */
#include <math.h>

#include "xmath.h"

double (atan2)(double y, double x)
{
	double z;
	const short errx = _Dtest(&x);
	const short erry = _Dtest(&y);
	int idx;

	if (errx <= 0 && erry <= 0) {	/* x and y finite or zero */
		if (DSIGN(y)) {
			y = -y;
			idx = 8;
		} else {
			idx = 0;
		}
		if (DSIGN(x)) {
			x = -x;
			idx ^= 6;
		}
		if (x < y) {
			z = x / y;
			idx ^= 2;
		} else {
			z = errx < 0 ? y / x : 0.0;
		}
	} else if (erry == _NANCODE) {
		return y;
	} else if (errx == _NANCODE) {
		return x;
	} else {	/* at least one infinity */
		z = errx == erry ? 1.0 : 0.0;
		idx = DSIGN(y) ? 8 : 0;
		if (DSIGN(x))
			idx ^= 6;
		if (erry == _INFCODE)
			idx ^= 2;
	}
	return _Atan(z, idx);
}
