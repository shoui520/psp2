/*
 * _Feraise function: raise the floating-point exceptions and report the
 * error in errno, EDOM for invalid or divide by zero, ERANGE for overflow or
 * underflow (which are also inexact).
 */
#include <errno.h>
#include <fenv.h>

#include "xmath.h"

void _Feraise(int except)
{
	if (except & (_FE_OVERFLOW | _FE_UNDERFLOW))
		except |= _FE_INEXACT;
	feraiseexcept(except);
	if (except & (_FE_DIVBYZERO | _FE_INVALID))
		errno = EDOM;
	else if (except & (_FE_OVERFLOW | _FE_UNDERFLOW))
		errno = ERANGE;
}
