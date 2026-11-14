/*
 * _LDsign function: whether the long double x is negative, for signbit.
 */
#include "xmath.h"

int _LDsign(long double x)
{
	return LSIGN(x) != 0;
}
