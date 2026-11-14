/*
 * _LDtest function: classify a long double, a double on psp2.
 */
#include "xmath.h"

short _LDtest(long double *px)
{
	return _Dtest((double *)px);
}
