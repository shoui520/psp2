/*
 * _WGetfloat function: convert a wscanf floating-point field, decimal or
 * hexadecimal (0x, with a binary exponent p), or inf, infinity, nan and
 * nan(n-char-sequence). The significant digits are gathered into a buffer,
 * the digits past FMAX counted as a power of the base, and the buffer is
 * converted with _WStold and stored as it is, without _Getfloat's rounding
 * through _LDunscale.
 */
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "xmath.h"
#include "xwstdio.h"

#define FMAX	48	/* the significant digits kept */
#define EMAX	8	/* the exponent digits kept */

#define BAD_ARGUMENT()	(EOF - _EXT1_FAIL("scanf_s: bad floating-point argument"))

/* store val as a T, at pans or through the next argument */
#define STORE_FLOAT(px, pans, T, val)					\
	do {								\
		if ((pans) != NULL) {					\
			*(T *)(pans) = (T)(val);			\
		} else {						\
			T *p = va_arg((px)->ap, T *);			\
									\
			if (p == NULL)					\
				return BAD_ARGUMENT();			\
			*p = (T)(val);					\
		}							\
	} while (0)

int _WGetfloat(_WSft *px, void *pans)
{
	wchar_t buf[FMAX + EMAX + 16];	/* sign, 0x, digits, point, exponent, NUL */
	wchar_t *p;
	wint_t ch;
	int nsig;
	int seen = 0;
	long pten;
	size_t ndigits = 10;	/* the digits of the base in "0123456789abcdefABCDEF" */
	static const wchar_t digits[] = L"0123456789abcdefABCDEF";

	px->nget = 0 < px->width ? px->width : INT_MAX;
	p = buf;
	ch = GETN(px);
	if (ch == L'+' || ch == L'-') {	/* the sign */
		*p++ = ch;
		ch = GETN(px);
	}
	if (ch == L'0') {	/* the hexadecimal prefix, or a zero */
		ch = GETN(px);
		if (ch == L'x' || ch == L'X') {
			*p++ = L'0';
			*p++ = L'x';
			ch = GETN(px);
			ndigits = 22;
			seen = 0;
		} else {
			seen = 1;
		}
	} else if (ch == L'n' || ch == L'N') {	/* nan or nan(n-char-sequence) */
		ndigits = 0;
		*p++ = L'n';
		if ((ch = GETN(px)) != 'a' && ch != L'A') {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = L'a';
		if ((ch = GETN(px)) != 'n' && ch != L'N') {
			UNGETN(px, ch);
			goto fail;
		}
		if ((ch = GETN(px)) != L'(') {
			UNGETN(px, ch);
			*p++ = L'n';
			goto convert;
		}
		do {	/* skip the n-char-sequence */
			ch = GETN(px);
		} while (iswalnum(ch) || ch == L'_');
		if (ch == L')') {
			*p++ = L'n';
			goto convert;
		}
		UNGETN(px, ch);
		goto fail;
	} else if (ch == L'i' || ch == L'I') {	/* inf or infinity */
		ndigits = 0;
		*p++ = L'i';
		if ((ch = GETN(px)) != 'n' && ch != L'N') {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = L'n';
		if ((ch = GETN(px)) != 'f' && ch != L'F') {
			UNGETN(px, ch);
			goto fail;
		}
		if ((ch = GETN(px)) != 'i' && ch != L'I') {	/* inf */
			UNGETN(px, ch);
			*p++ = L'f';
			pten = 0;
			goto convert;
		}
		if (((ch = GETN(px)) != 'n' && ch != L'N')
		    || ((ch = GETN(px)) != 'i' && ch != L'I')
		    || ((ch = GETN(px)) != 't' && ch != L'T')
		    || ((ch = GETN(px)) != 'y' && ch != L'Y')) {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = L'f';	/* infinity */
		pten = 0;
		goto convert;
	}

	for (; ch == L'0'; seen = 1)	/* leading zeros as one */
		ch = GETN(px);
	if (seen)
		*p++ = L'0';
	pten = 0;
	for (nsig = 0; ch != WEOF && wmemchr(digits, ch, ndigits) != NULL; ch = GETN(px), seen = 1) {
		if (nsig < FMAX)
			*p++ = ch, ++nsig;
		else
			++pten;
	}
	if (_Wctob(ch) == '.') {	/* the fraction */
		*p++ = ch;
		ch = GETN(px);
	}
	if (nsig == 0) {	/* the zeros after the point count down the power */
		for (; ch == L'0'; ch = GETN(px), seen = 1)
			--pten;
		if (pten < 0)
			*p++ = L'0', ++pten;
	}
	for (; ch != WEOF && wmemchr(digits, ch, ndigits) != NULL; ch = GETN(px), seen = 1) {
		if (nsig < FMAX)
			*p++ = ch, ++nsig;
	}
	if (seen && ((ndigits == 10 && (ch == L'e' || ch == L'E'))
		     || (ndigits != 10 && (ch == L'p' || ch == L'P')))) {	/* the exponent */
		int nexp;

		*p++ = ch;
		ch = GETN(px);
		if (ch == L'+' || ch == L'-') {
			*p++ = ch;
			ch = GETN(px);
		}
		if (ch == L'0') {	/* leading zeros as one */
			do {
				ch = GETN(px);
			} while (ch == L'0');
			*p++ = L'0';
		}
		for (nexp = 0, seen = 1; iswdigit(ch); ch = GETN(px)) {
			if (nexp < EMAX)
				*p++ = ch, ++nexp;
		}
	}
	UNGETN(px, ch);
	if (!seen)
		goto fail;

convert:
	*p = L'\0';
	if (!px->noconv) {	/* convert and store */
		long double ldval;

		if (10 < (int)ndigits) {
			ldval = _WStold(buf, NULL, 0);
			_LDscale(&ldval, pten * 4);
		} else {
			ldval = _WStold(buf, NULL, pten);
		}
		px->stored = 1;
		if (px->qual == L'l')
			STORE_FLOAT(px, pans, double, ldval);
		else if (px->qual == L'L')
			STORE_FLOAT(px, pans, long double, ldval);
		else
			STORE_FLOAT(px, pans, float, ldval);
	}
	return 1;

fail:
	return p == buf && ch == WEOF ? EOF : 0;
}
