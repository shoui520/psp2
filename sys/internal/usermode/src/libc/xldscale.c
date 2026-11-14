/*
 * _LDscale function: *px times 2^lexp, with checking (long double is double).
 */
#include "xmath.h"

short _LDscale(long double *px, long lexp)
{
	return _Dscale((double *)px, lexp);
}
