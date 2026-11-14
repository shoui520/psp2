/*
 * xfvalues.c: the float constants of <math.h> and the library (IEEE 754; the words of a
 * _Dconst are little-endian, the exponent in word _F0)
 */
#include <math.h>

#include "xmath.h"

#define NBITS		(16 + _FOFF)	/* the fraction's bits */
#define INIT(w0)	{0, w0}
#define INIT2(w0, w1)	{w1, w0}

const _Dconst _FDenorm = {INIT2(0, 1)};
const _Dconst _FEps = {INIT((_FBIAS - NBITS - 1) << _FOFF)};
const _Dconst _FInf = {INIT(_FMAX << _FOFF)};
const _Dconst _FNan = {INIT((_FMAX << _FOFF) | (1 << (_FOFF - 1)))};
const _Dconst _FSnan = {INIT2(_FMAX << _FOFF, 1)};
const _Dconst _FRteps = {INIT((_FBIAS - NBITS / 2) << _FOFF)};
const float _FXbig = (NBITS + 1) * 347L / 1000;
const float _FZero = 0.0F;
