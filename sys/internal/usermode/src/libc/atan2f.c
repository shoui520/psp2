/*
 * atan2f function: the arctangent, in float, of y / x in the quadrant of (x, y), through
 * _FAtan of the smaller over the larger magnitude.
 */
#include <math.h>

#include "xmath.h"

float (atan2f)(float y, float x)
{
	float z;
	const short errx = _FDtest(&x);
	const short erry = _FDtest(&y);
	int idx;

	if (errx <= 0 && erry <= 0) {	/* x and y finite or zero */
		if (FSIGN(y)) {
			y = -y;
			idx = 8;
		} else {
			idx = 0;
		}
		if (FSIGN(x)) {
			x = -x;
			idx ^= 6;
		}
		if (x < y) {
			z = x / y;
			idx ^= 2;
		} else {
			z = errx < 0 ? y / x : 0.0F;
		}
	} else if (erry == _NANCODE) {
		return y;
	} else if (errx == _NANCODE) {
		return x;
	} else {	/* at least one infinity */
		z = errx == erry ? 1.0F : 0.0F;
		idx = FSIGN(y) ? 8 : 0;
		if (FSIGN(x))
			idx ^= 6;
		if (erry == _INFCODE)
			idx ^= 2;
	}
	return _FAtan(z, idx);
}
