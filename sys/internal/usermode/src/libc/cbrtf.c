/*
 * cbrtf function: the cube root in float, with one Newton step.
 */
#include <math.h>

#include "xmath.h"

float (cbrtf)(float x)
{
	short xexp;

	switch (_FDunscale(&xexp, &x)) {
	case 0:
	case _INFCODE:
	case _NANCODE:
		return x;
	default: {	/* finite: 0.5 <= |x| < 1 */
		float z;
		int i;
		int neg;
		long n;

		for (i = 0; xexp / 3 * 3 != xexp; ++xexp, --i)
			;
		if (i < 0)
			_FDscale(&x, i);
		n = xexp / 3;
		neg = 0;
		if (FSIGN(x)) {
			x = -x;
			neg = 1;
		}
		z = ((0.24379F * x + 0.95807F) * x + 0.07892F) / (x + 0.27962F);
		z = 0.5F * (z + 1.5F * x / (z * z + 0.5F * x / z));
		if (neg)
			z = -z;
		_FDscale(&z, n);
		return z;
	}
	}
}
