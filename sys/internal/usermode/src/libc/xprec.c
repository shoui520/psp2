/*
 * The extended-precision helpers for double: terms of 26 bits.
 */
#define FTYPE		double
#define FNAME(x)	_##x
#define FCONST(x)	_##x._Double
#define FLIT(x)		x
#define FSIGNBIT(x)	DSIGN(x)
#define FLDEXP		ldexp
#define FSQRT		sqrt
#define FBITS		53
#define BITS_WORD	26
#define BIG_EXP		2048

#include <math.h>

#include "xxxprec.h"
