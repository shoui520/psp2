/*
 * The extended-precision helpers for long double (a double on psp2): terms
 * of 26 bits.
 */
#define FTYPE		long double
#define FNAME(x)	_L##x
#define FCONST(x)	_L##x._Long_double
#define FLIT(x)		x##L
#define FSIGNBIT(x)	LSIGN(x)
#define FLDEXP		ldexpl
#define FSQRT		sqrtl
#define BITS_WORD	26
#define BIG_EXP		2048

#include <math.h>

#include "xxxprec.h"
