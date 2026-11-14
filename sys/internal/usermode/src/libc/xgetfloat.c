/*
 * _Getfloat function: convert a scanf floating-point field, decimal or
 * hexadecimal (0x, with a binary exponent p), or inf, infinity, nan and
 * nan(n-char-sequence). The significant digits are gathered into a buffer,
 * the digits past FMAX counted as a power of the base, and the buffer is
 * converted with _Stold.
 */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "xmath.h"
#include "xstdio.h"

#define FMAX	48	/* the significant digits kept */
#define EMAX	8	/* the exponent digits kept */

int _Getfloat(_Sft *px, void *pans)
{
	char buf[FMAX + EMAX + 16];	/* sign, 0x, digits, point, exponent, NUL */
	char *p;
	int ch;
	int nsig;
	int seen = 0;
	long pten;
	size_t ndigits = 10;	/* the digits of the base in "0123456789abcdefABCDEF" */
	static const char digits[] = "0123456789abcdefABCDEF";

	px->nget = 0 < px->width ? px->width : INT_MAX;
	p = buf;
	ch = GETN(px);
	if (ch == '+' || ch == '-') {	/* the sign */
		*p++ = ch;
		ch = GETN(px);
	}
	if (ch == '0') {	/* the hexadecimal prefix, or a zero */
		ch = GETN(px);
		if (ch == 'x' || ch == 'X') {
			*p++ = '0';
			*p++ = 'x';
			ch = GETN(px);
			ndigits = 22;
			seen = 0;
		} else {
			seen = 1;
		}
	} else if (ch == 'n' || ch == 'N') {	/* nan or nan(n-char-sequence) */
		ndigits = 0;
		*p++ = 'n';
		if ((ch = GETN(px)) != 'a' && ch != 'A') {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = 'a';
		if ((ch = GETN(px)) != 'n' && ch != 'N') {
			UNGETN(px, ch);
			goto fail;
		}
		if ((ch = GETN(px)) != '(') {
			UNGETN(px, ch);
			*p++ = 'n';
			goto convert;
		}
		do {	/* skip the n-char-sequence */
			ch = GETN(px);
		} while (isalnum(ch) || ch == '_');
		if (ch == ')') {
			*p++ = 'n';
			goto convert;
		}
		UNGETN(px, ch);
		goto fail;
	} else if (ch == 'i' || ch == 'I') {	/* inf or infinity */
		ndigits = 0;
		*p++ = 'i';
		if ((ch = GETN(px)) != 'n' && ch != 'N') {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = 'n';
		if ((ch = GETN(px)) != 'f' && ch != 'F') {
			UNGETN(px, ch);
			goto fail;
		}
		if ((ch = GETN(px)) != 'i' && ch != 'I') {	/* inf */
			UNGETN(px, ch);
			*p++ = 'f';
			pten = 0;
			goto convert;
		}
		if (((ch = GETN(px)) != 'n' && ch != 'N')
		    || ((ch = GETN(px)) != 'i' && ch != 'I')
		    || ((ch = GETN(px)) != 't' && ch != 'T')
		    || ((ch = GETN(px)) != 'y' && ch != 'Y')) {
			UNGETN(px, ch);
			goto fail;
		}
		*p++ = 'f';	/* infinity */
		pten = 0;
		goto convert;
	}

	for (; ch == '0'; seen = 1)	/* leading zeros as one */
		ch = GETN(px);
	if (seen)
		*p++ = '0';
	pten = 0;
	for (nsig = 0; ch != EOF && memchr(digits, ch, ndigits) != NULL; ch = GETN(px), seen = 1) {
		if (nsig < FMAX)
			*p++ = ch, ++nsig;
		else
			++pten;
	}
	if (ch == '.') {	/* the fraction */
		*p++ = '.';
		ch = GETN(px);
	}
	if (nsig == 0) {	/* the zeros after the point count down the power */
		for (; ch == '0'; ch = GETN(px), seen = 1)
			--pten;
		if (pten < 0)
			*p++ = '0', ++pten;
	}
	for (; ch != EOF && memchr(digits, ch, ndigits) != NULL; ch = GETN(px), seen = 1) {
		if (nsig < FMAX)
			*p++ = ch, ++nsig;
	}
	if (seen && ((ndigits == 10 && (ch == 'e' || ch == 'E'))
		     || (ndigits != 10 && (ch == 'p' || ch == 'P')))) {	/* the exponent */
		int nexp;

		*p++ = ch;
		ch = GETN(px);
		if (ch == '+' || ch == '-') {
			*p++ = ch;
			ch = GETN(px);
		}
		if (ch == '0') {	/* leading zeros as one */
			do {
				ch = GETN(px);
			} while (ch == '0');
			*p++ = '0';
		}
		for (nexp = 0, seen = 1; isdigit(ch); ch = GETN(px)) {
			if (nexp < EMAX)
				*p++ = ch, ++nexp;
		}
	}
	UNGETN(px, ch);
	if (!seen)
		goto fail;

convert:
	*p = '\0';
	if (!px->noconv) {	/* convert and store */
		long double ldval;

		if (10 < (int)ndigits) {
			ldval = _Stold(buf, NULL, 0);
			_LDscale(&ldval, pten * 4);
		} else {
			ldval = _Stold(buf, NULL, pten);
		}
		px->stored = 1;
		if (pans == NULL && (pans = va_arg(px->ap, void *)) == NULL)
			return EOF - _EXT1_FAIL("scanf_s: bad floating-point argument");
		if (px->qual == 'l') {	/* a double, rounded once */
			short xexp;

			if (_LDunscale(&xexp, &ldval) < 0) {
				double dval = ldval;

				_Dscale(&dval, xexp);
				*(double *)pans = dval;
			} else {
				*(double *)pans = ldval;
			}
		} else if (px->qual == 'L') {
			*(long double *)pans = ldval;
		} else {	/* a float, rounded once */
			short xexp;

			if (_LDunscale(&xexp, &ldval) < 0) {
				float fval = ldval;

				_FDscale(&fval, xexp);
				*(float *)pans = fval;
			} else {
				*(float *)pans = ldval;
			}
		}
	}
	return 1;

fail:
	return p == buf && ch == EOF ? EOF : 0;
}
