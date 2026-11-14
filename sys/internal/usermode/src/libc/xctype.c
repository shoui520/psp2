/*
 * xctype.c: the C locale's character classes, _Ctype[c] for c from EOF to UCHAR_MAX
 * (ASCII; the codes above 0x7f have none)
 */
#include <ctype.h>
#include <limits.h>

#define XDI	(_DI|_XD)	/* '0'-'9' */
#define XLO	(_LO|_XD)	/* 'a'-'f' */
#define XUP	(_UP|_XD)	/* 'A'-'F' */

static const short ctype_tab[257] = {0,	/* EOF */
	_BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
	_BB, _XB|_BB|_CN, _BB|_CN, _BB|_CN, _BB|_CN, _BB|_CN, _BB, _BB,
	_BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
	_BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
	_SP, _PU, _PU, _PU, _PU, _PU, _PU, _PU,
	_PU, _PU, _PU, _PU, _PU, _PU, _PU, _PU,
	XDI, XDI, XDI, XDI, XDI, XDI, XDI, XDI,
	XDI, XDI, _PU, _PU, _PU, _PU, _PU, _PU,
	_PU, XUP, XUP, XUP, XUP, XUP, XUP, _UP,
	_UP, _UP, _UP, _UP, _UP, _UP, _UP, _UP,
	_UP, _UP, _UP, _UP, _UP, _UP, _UP, _UP,
	_UP, _UP, _UP, _PU, _PU, _PU, _PU, _PU,
	_PU, XLO, XLO, XLO, XLO, XLO, XLO, _LO,
	_LO, _LO, _LO, _LO, _LO, _LO, _LO, _LO,
	_LO, _LO, _LO, _LO, _LO, _LO, _LO, _LO,
	_LO, _LO, _LO, _PU, _PU, _PU, _PU, _BB,
};	/* 0x80-0xff: no class */

const _Ctype_t _Ctype = &ctype_tab[1];
