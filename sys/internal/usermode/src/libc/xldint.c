/*
 * _LDint function: clear the fraction bits of *px below 2^xexp in long
 * double, a double on psp2.
 */
#include "xmath.h"

short _LDint(long double *px, short xexp)
{
	return _Dint((double *)px, xexp);
}
