/*
 * _Stolx function: a string to long, through _Stoulx.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

long _Stolx(const char *s, char **endptr, int base, int *perr)
{
	const char *sc;
	char *se, sign;
	unsigned long x;

	if (endptr == NULL)
		endptr = &se;
	for (sc = s; isspace((unsigned char)*sc); ++sc)
		;
	sign = *sc == '-' || *sc == '+' ? *sc++ : '+';
	x = _Stoulx(sc, endptr, base, perr);
	if (sc == *endptr) {		/* no digits */
		*endptr = (char *)s;
		return 0;
	}
	if ((sign == '+' && LONG_MAX < x) || (sign == '-' && 0 - (unsigned long)LONG_MIN < x)) {
		errno = ERANGE;
		if (perr != NULL)
			*perr = 1;
		return sign == '-' ? LONG_MIN : LONG_MAX;
	}
	return (long)(sign == '-' ? 0 - x : x);
}
