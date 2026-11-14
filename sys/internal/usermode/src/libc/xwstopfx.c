/*
 * _WStopfx function: the prefix of a floating-point field (sign, nan, inf or 0x).
 */
#include <wchar.h>
#include <wctype.h>
#include <stddef.h>

#include "xmath.h"

int _WStopfx(const wchar_t **ps, wchar_t **endptr)
{
	const wchar_t *s = *ps;
	int code = 0;

	for (; iswspace(*s); ++s)
		;
	if (*s == '-') {
		code = FL_NEG;
		++s;
	} else if (*s == '+') {
		++s;
	}
	if (*s == 'n' || *s == 'N') {		/* "nan" or fail */
		if ((*++s != 'a' && *s != 'A') || (*++s != 'n' && *s != 'N')) {
			s = *ps;
			code = FL_ERR;
		} else {			/* an optional (n-char-sequence) */
			const wchar_t *q = ++s;

			code = FL_NAN;
			if (*q == '(') {
				while (iswalnum(*++q) || *q == '_')
					;
				if (*q == ')')
					s = ++q;
			}
		}
		if (endptr != NULL)
			*endptr = (wchar_t *)s;
	} else if (*s == 'i' || *s == 'I') {	/* "inf" or fail */
		if ((*++s != 'n' && *s != 'N') || (*++s != 'f' && *s != 'F')) {
			s = *ps;
			code = FL_ERR;
		} else {			/* the optional rest of "infinity" */
			const wchar_t *q = ++s;

			code |= FL_INF;
			if ((*q == 'i' || *q == 'I')
			    && (*++q == 'n' || *q == 'N')
			    && (*++q == 'i' || *q == 'I')
			    && (*++q == 't' || *q == 'T')
			    && (*++q == 'y' || *q == 'Y'))
				s = ++q;
		}
		if (endptr != NULL)
			*endptr = (wchar_t *)s;
	} else if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {	/* a hexadecimal field */
		const wchar_t *s1 = s + 2;

		if (*s1 == '.')
			++s1;
		if (!iswxdigit(*s1)) {
			code |= FL_DEC;
		} else {
			s += 2;
			code |= FL_HEX;
		}
	} else {
		code |= FL_DEC;
	}
	*ps = s;
	return code;
}
