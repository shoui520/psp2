/*
 * _LDunscale function: separate *px into a fraction in [0.5, 1) and a power of
 * two (long double is double).
 */
#include "xmath.h"

short _LDunscale(short *pex, long double *px)
{
	return _Dunscale(pex, (double *)px);
}
