/*
 * xvalues.c: the double constants of <math.h> and the library (IEEE 754; the words of a
 * _Dconst are little-endian, the exponent in word _D0)
 */
#include <math.h>

#include "xmath.h"

#define NBITS		(48 + _DOFF)	/* the fraction's bits */
#define INIT(w0)	{0, 0, 0, w0}
#define INIT2(w0, w1)	{w1, 0, 0, w0}

const _Dconst _Denorm = {INIT2(0, 1)};
const _Dconst _Eps = {INIT((_DBIAS - NBITS - 1) << _DOFF)};
const _Dconst _Hugeval = {INIT(_DMAX << _DOFF)};
const _Dconst _Inf = {INIT(_DMAX << _DOFF)};
const _Dconst _Nan = {INIT((_DMAX << _DOFF) | (1 << (_DOFF - 1)))};
const _Dconst _Snan = {INIT2(_DMAX << _DOFF, 1)};
const _Dconst _Rteps = {INIT((_DBIAS - NBITS / 2) << _DOFF)};
const double _Xbig = (NBITS + 1) * 347L / 1000;
const double _Zero = 0.0;
