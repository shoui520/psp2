/*
 * cbrt function: the cube root. The exponent is made a multiple of three,
 * a rational guess is refined by two Newton steps, and the exponent's third
 * is put back.
 */
#include <math.h>

#include "xmath.h"

double (cbrt)(double x)
{
	short xexp;

	switch (_Dunscale(&xexp, &x)) {
	case 0:
	case _INFCODE:
	case _NANCODE:
		return x;
	default: {	/* finite: 0.5 <= |x| < 1 */
		double z;
		int i;
		int neg;
		short n;

		for (i = 0; xexp / 3 * 3 != xexp; ++xexp, --i)
			;
		if (i < 0)
			_Dscale(&x, i);
		n = xexp / 3;
		neg = 0;
		if (DSIGN(x)) {
			x = -x;
			neg = 1;
		}
		z = ((0.24379 * x + 0.95807) * x + 0.07892) / (x + 0.27962);
		z = 0.5 * (z + 1.5 * x / (z * z + 0.5 * x / z));
		z = 0.5 * (z + 1.5 * x / (z * z + 0.5 * x / z));
		if (neg)
			z = -z;
		_Dscale(&z, n);
		return z;
	}
	}
}
