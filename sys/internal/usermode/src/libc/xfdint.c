/*
 * _FDint function: clear the fraction bits of *px below 2^xexp in float;
 * return the class of *px, or whether any bits were dropped.
 */
#include <stddef.h>

#include "xmath.h"

short _FDint(float *px, short xexp)
{
	_Fval *ps = (_Fval *)(char *)px;
	unsigned short frac;
	short xchar = (short)((ps->_Sh[_F0] & _FMASK) >> _FOFF);

	if (xchar == _FMAX) {	/* NaN or INF */
		return (short)((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0
			       ? _NANCODE : _INFCODE);
	} else if ((ps->_Sh[_F0] & ~_FSIGN) == 0 && ps->_Sh[_F1] == 0) {	/* zero */
		return 0;
	}
	xchar = (short)((_FBIAS + 16 + _FOFF + 1) - xchar - xexp);
	if (xchar <= 0) {	/* no fraction bits to drop */
		return 0;
	} else if ((16 + _FOFF) < xchar) {	/* all of them: +/-0 */
		ps->_Sh[_F0] &= _FSIGN;
		ps->_Sh[_F1] = 0;
		return _FINITE;
	} else {	/* some of them */
		static const unsigned short mask[] = {
			0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f,
			0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff,
		};
		static const size_t sub[] = {_F1, _F0};

		frac = mask[xchar & 0xf];
		xchar >>= 4;
		frac &= ps->_Sh[sub[xchar]];
		ps->_Sh[sub[xchar]] ^= frac;
		if (0 < xchar) {	/* the whole word below */
			frac |= ps->_Sh[_F1];
			ps->_Sh[_F1] = 0;
		}
		return frac != 0 ? _FINITE : 0;
	}
}
