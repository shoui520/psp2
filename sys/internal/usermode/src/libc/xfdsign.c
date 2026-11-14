/*
 * _FDsign function: whether x is negative, for signbit, in float.
 */
#include "xmath.h"

int _FDsign(float x)
{
	return FSIGN(x) != 0;
}
