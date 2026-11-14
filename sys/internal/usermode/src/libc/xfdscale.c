/*
 * _FDscale function: *px times 2^lexp, with checking.
 */
#include "xmath.h"

short _FDscale(float *px, long lexp)
{
	_Fval *ps = (_Fval *)(char *)px;
	short xchar = (short)((ps->_Sh[_F0] & _FMASK) >> _FOFF);

	if (xchar == _FMAX)	/* inf or nan */
		return (short)((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0
			       ? _NANCODE : _INFCODE);
	else if (xchar == 0 && 0 < (xchar = _FDnorm(ps)))	/* zero */
		return 0;

	if (0 < lexp && _FMAX - xchar <= lexp) {	/* overflow: +/-inf */
		*px = ps->_Sh[_F0] & _FSIGN ? -_FInf._Float : _FInf._Float;
		return _INFCODE;
	} else if (-xchar < lexp) {	/* a finite result: repack */
		ps->_Sh[_F0] = (unsigned short)((ps->_Sh[_F0] & ~_FMASK) | (lexp + xchar) << _FOFF);
		return _FINITE;
	} else {			/* denormalized: scale */
		unsigned short sign = (unsigned short)(ps->_Sh[_F0] & _FSIGN);

		ps->_Sh[_F0] = (unsigned short)(1 << _FOFF | (ps->_Sh[_F0] & _FFRAC));
		lexp += xchar - 1;
		if (lexp < -(16 + 1 + _FOFF) || 0 <= lexp) {	/* underflow: +/-0 */
			ps->_Sh[_F0] = sign;
			ps->_Sh[_F1] = 0;
			return 0;
		} else {		/* nonzero: align the fraction */
			short xexp = (short)lexp;
			unsigned short psx = 0;

			if (xexp <= -16) {	/* by words */
				psx = ps->_Sh[_F1];
				ps->_Sh[_F1] = ps->_Sh[_F0];
				ps->_Sh[_F0] = 0;
				xexp += 16;
			}
			if ((xexp = (short)-xexp) != 0) {	/* by bits */
				psx = (ps->_Sh[_F1] << (16 - xexp)) | (psx != 0 ? 1 : 0);
				ps->_Sh[_F1] = (unsigned short)(ps->_Sh[_F1] >> xexp
								| ps->_Sh[_F0] << (16 - xexp));
				ps->_Sh[_F0] >>= xexp;
			}

			ps->_Sh[_F0] |= sign;
			if ((0x8000 < psx || (0x8000 == psx && (ps->_Sh[_F1] & 0x0001) != 0))
			    && (++ps->_Sh[_F1] & 0xffff) == 0)
				++ps->_Sh[_F0];	/* round up */
			else if (ps->_Sh[_F0] == sign && ps->_Sh[_F1] == 0)
				return 0;
			return _FINITE;
		}
	}
}
