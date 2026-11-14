/*
 * _Dscale function: *px times 2^lexp, with checking.
 */
#include "xmath.h"

short _Dscale(double *px, long lexp)
{
	_Dval *ps = (_Dval *)(char *)px;
	short xchar = (short)((ps->_Sh[_D0] & _DMASK) >> _DOFF);

	if (xchar == _DMAX)	/* inf or nan */
		return (short)((ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] != 0
			       || ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0 ? _NANCODE : _INFCODE);
	else if (xchar == 0 && 0 < (xchar = _Dnorm(ps)))	/* zero */
		return 0;

	if (0 < lexp && _DMAX - xchar <= lexp) {	/* overflow: +/-inf */
		*px = ps->_Sh[_D0] & _DSIGN ? -_Inf._Double : _Inf._Double;
		return _INFCODE;
	} else if (-xchar < lexp) {	/* a finite result: repack */
		ps->_Sh[_D0] = (unsigned short)((ps->_Sh[_D0] & ~_DMASK) | (lexp + xchar) << _DOFF);
		return _FINITE;
	} else {			/* denormalized: scale */
		unsigned short sign = (unsigned short)(ps->_Sh[_D0] & _DSIGN);

		ps->_Sh[_D0] = (unsigned short)(1 << _DOFF | (ps->_Sh[_D0] & _DFRAC));
		lexp += xchar - 1;
		if (lexp < -(48 + 1 + _DOFF) || 0 <= lexp) {	/* underflow: +/-0 */
			ps->_Sh[_D0] = sign;
			ps->_Sh[_D1] = 0;
			ps->_Sh[_D2] = 0;
			ps->_Sh[_D3] = 0;
			return 0;
		} else {		/* nonzero: align the fraction */
			short xexp = (short)lexp;
			unsigned short psx = 0;

			for (; xexp <= -16; xexp += 16) {	/* by words */
				psx = ps->_Sh[_D3] | (psx != 0 ? 1 : 0);
				ps->_Sh[_D3] = ps->_Sh[_D2];
				ps->_Sh[_D2] = ps->_Sh[_D1];
				ps->_Sh[_D1] = ps->_Sh[_D0];
				ps->_Sh[_D0] = 0;
			}
			if ((xexp = (short)-xexp) != 0) {	/* by bits */
				psx = (ps->_Sh[_D3] << (16 - xexp)) | (psx != 0 ? 1 : 0);
				ps->_Sh[_D3] = (unsigned short)(ps->_Sh[_D3] >> xexp
								| ps->_Sh[_D2] << (16 - xexp));
				ps->_Sh[_D2] = (unsigned short)(ps->_Sh[_D2] >> xexp
								| ps->_Sh[_D1] << (16 - xexp));
				ps->_Sh[_D1] = (unsigned short)(ps->_Sh[_D1] >> xexp
								| ps->_Sh[_D0] << (16 - xexp));
				ps->_Sh[_D0] >>= xexp;
			}

			ps->_Sh[_D0] |= sign;
			if ((0x8000 < psx || (0x8000 == psx && (ps->_Sh[_D3] & 0x0001) != 0))
			    && (++ps->_Sh[_D3] & 0xffff) == 0
			    && (++ps->_Sh[_D2] & 0xffff) == 0
			    && (++ps->_Sh[_D1] & 0xffff) == 0)
				++ps->_Sh[_D0];	/* round up */
			else if (ps->_Sh[_D0] == sign && ps->_Sh[_D1] == 0
				 && ps->_Sh[_D2] == 0 && ps->_Sh[_D3] == 0)
				return 0;
			return _FINITE;
		}
	}
}
