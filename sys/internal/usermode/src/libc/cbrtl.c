/*
 * cbrtl function: the cube root in long double. The exponent is made a multiple of three,
 * a rational guess is refined by two Newton steps, and the exponent's third
 * is put back.
 */
#include <math.h>

#include "xmath.h"

long double (cbrtl)(long double x)
{
	short xexp;

	switch (_LDunscale(&xexp, &x)) {
	case 0:
	case _INFCODE:
	case _NANCODE:
		return x;
	default: {	/* finite: 0.5L <= |x| < 1 */
		long double z;
		int i;
		int neg;
		long n;

		for (i = 0; xexp / 3 * 3 != xexp; ++xexp, --i)
			;
		if (i < 0)
			_LDscale(&x, i);
		n = xexp / 3;
		neg = 0;
		if (LSIGN(x)) {
			x = -x;
			neg = 1;
		}
		z = ((0.24379L * x + 0.95807L) * x + 0.07892L) / (x + 0.27962L);
		z = 0.5L * (z + 1.5L * x / (z * z + 0.5L * x / z));
		z = 0.5L * (z + 1.5L * x / (z * z + 0.5L * x / z));
		if (neg)
			z = -z;
		_LDscale(&z, n);
		return z;
	}
	}
}
