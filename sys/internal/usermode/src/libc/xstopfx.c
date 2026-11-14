/*
 * _Stopfx function: the prefix of a floating-point field (sign, nan, inf or 0x).
 */
#include <ctype.h>
#include <stddef.h>

#include "xmath.h"

int _Stopfx(const char **ps, char **endptr)
{
	const char *s = *ps;
	int code = 0;

	for (; isspace((unsigned char)*s); ++s)
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
			const char *q = ++s;

			code = FL_NAN;
			if (*q == '(') {
				while (isalnum((unsigned char)*++q) || *q == '_')
					;
				if (*q == ')')
					s = ++q;
			}
		}
		if (endptr != NULL)
			*endptr = (char *)s;
	} else if (*s == 'i' || *s == 'I') {	/* "inf" or fail */
		if ((*++s != 'n' && *s != 'N') || (*++s != 'f' && *s != 'F')) {
			s = *ps;
			code = FL_ERR;
		} else {			/* the optional rest of "infinity" */
			const char *q = ++s;

			code |= FL_INF;
			if ((*q == 'i' || *q == 'I')
			    && (*++q == 'n' || *q == 'N')
			    && (*++q == 'i' || *q == 'I')
			    && (*++q == 't' || *q == 'T')
			    && (*++q == 'y' || *q == 'Y'))
				s = ++q;
		}
		if (endptr != NULL)
			*endptr = (char *)s;
	} else if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {	/* a hexadecimal field */
		const char *s1 = s + 2;

		if (*s1 == '.')
			++s1;
		if (!isxdigit((unsigned char)*s1)) {
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
