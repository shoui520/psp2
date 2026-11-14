/*
 * fabsl function: the magnitude of x in long double.
 */
#include <math.h>

#include "xmath.h"

long double (fabsl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return _LInf._Long_double;
	case 0:
		return 0.0L;
	default:
		return x < 0.0L ? -x : x;
	}
}
