/*
 * The extended-precision helpers for float: terms of 12 bits.
 */
#define FTYPE		float
#define FNAME(x)	_F##x
#define FCONST(x)	_F##x._Float
#define FLIT(x)		x##F
#define FSIGNBIT(x)	FSIGN(x)
#define FLDEXP		ldexpf
#define FSQRT		sqrtf
#define BITS_WORD	12
#define BIG_EXP		256

#include <math.h>

#include "xxxprec.h"
