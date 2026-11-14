/*
 * _Dint function: clear the fraction bits of *px below 2^xexp; return the
 * class of *px, or whether any bits were dropped.
 */
#include <stddef.h>

#include "xmath.h"

short _Dint(double *px, short xexp)
{
	_Dval *ps = (_Dval *)(char *)px;
	unsigned short frac;
	short xchar = (short)((ps->_Sh[_D0] & _DMASK) >> _DOFF);

	if (xchar == _DMAX) {	/* NaN or INF */
		return (short)((ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] != 0
			       || ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0 ? _NANCODE : _INFCODE);
	} else if ((ps->_Sh[_D0] & ~_DSIGN) == 0 && ps->_Sh[_D1] == 0
		   && ps->_Sh[_D2] == 0 && ps->_Sh[_D3] == 0) {	/* zero */
		return 0;
	}
	xchar = (short)((_DBIAS + 48 + _DOFF + 1) - xchar - xexp);
	if (xchar <= 0) {	/* no fraction bits to drop */
		return 0;
	} else if ((48 + _DOFF) < xchar) {	/* all of them: +/-0 */
		ps->_Sh[_D0] &= _DSIGN;
		ps->_Sh[_D1] = 0;
		ps->_Sh[_D2] = 0;
		ps->_Sh[_D3] = 0;
		return _FINITE;
	} else {	/* some of them */
		static const unsigned short mask[] = {
			0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f,
			0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff,
		};
		static const size_t sub[] = {_D3, _D2, _D1, _D0};

		frac = mask[xchar & 0xf];
		xchar >>= 4;
		frac &= ps->_Sh[sub[xchar]];
		ps->_Sh[sub[xchar]] ^= frac;
		switch (xchar) {	/* the whole words below */
		case 3:
			frac |= ps->_Sh[_D1];
			ps->_Sh[_D1] = 0;
			/* fall through */
		case 2:
			frac |= ps->_Sh[_D2];
			ps->_Sh[_D2] = 0;
			/* fall through */
		case 1:
			frac |= ps->_Sh[_D3];
			ps->_Sh[_D3] = 0;
		}
		return frac != 0 ? _FINITE : 0;
	}
}
