/*
 * xlvalues.c: the long double constants of <math.h> and the library; long double is double
 * on psp2, so they are xvalues.c's
 */
#include <math.h>

#include "xmath.h"

#define NBITS		(48 + _DOFF)	/* the fraction's bits */
#define INIT(w0)	{0, 0, 0, w0}
#define INIT2(w0, w1)	{w1, 0, 0, w0}

const _Dconst _LDenorm = {INIT2(0, 1)};
const _Dconst _LEps = {INIT((_DBIAS - NBITS - 1) << _DOFF)};
const _Dconst _LInf = {INIT(_DMAX << _DOFF)};
const _Dconst _LNan = {INIT((_DMAX << _DOFF) | (1 << (_DOFF - 1)))};
const _Dconst _LSnan = {INIT2(_DMAX << _DOFF, 1)};
const _Dconst _LRteps = {INIT((_DBIAS - NBITS / 2) << _DOFF)};
const long double _LXbig = (NBITS + 1) * 347L / 1000;
const long double _LZero = 0.0L;
