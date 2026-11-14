/*
 * _WStofx function: a wide string to float, with the decimal exponent pten added, and
 * checking.
 */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>

#include "xmath.h"

float _WStofx(const wchar_t *s, wchar_t **endptr, long pten, int *perr)
{
	float x;
	long lo[1 + 1];
	const wchar_t *s0 = s;
	int code = _WStopfx(&s, endptr);
	const int neg = code & FL_NEG;

	if (perr != NULL)
		*perr = 0;
	if ((code &= ~FL_NEG) == FL_DEC) {	/* parse decimal digits */
		int nlo = _WStoflt(s0, s, endptr, lo, 1);

		if (nlo == 0 || lo[1] == 0)
			x = 0;
		else if (38 < (pten += lo[0])) {	/* certain overflow */
			x = _FInf._Float;
			errno = ERANGE;
			if (perr != NULL)
				*perr |= 1;
		} else if (pten < -46) {	/* certain underflow */
			x = 0;
			errno = ERANGE;
			if (perr != NULL)
				*perr |= 1;
		} else {	/* compute in the extended format */
			_Xfloat ten9 = { 0x401c, 0xee6b280000000000ULL };	/* 1e9 */
			_Xfloat xx = _Xfromd((double)lo[1]);
			int i;

			for (i = 1; i < nlo; ++i)
				xx = _Xadd(_Xmul(xx, ten9), _Xfromd((double)lo[i + 1]));
			if (pten != 0)
				xx = _Xtento(xx, pten);
			x = _Xtof(xx, perr);
		}
	} else if (code == FL_HEX) {	/* parse hexadecimal digits */
		int nlo = _WStoxflt(s0, s, endptr, lo, 1);

		if (nlo == 0)
			x = 0;
		else {
			int i;

			x = (float)lo[1];
			for (i = 1; i < nlo; ++i)
				x = x * 268435456.0F + (float)lo[i + 1];
		}
		_FDscale(&x, lo[0]);
		x = _FDtento((double)x, pten, perr);
	} else if (code == FL_INF)
		x = _FInf._Float;
	else if (code == FL_NAN)
		x = _FNan._Float;
	else
		x = 0;
	return neg ? -x : x;
}

float _WStof(const wchar_t *s, wchar_t **endptr, long pten)
{
	return _WStofx(s, endptr, pten, NULL);
}
